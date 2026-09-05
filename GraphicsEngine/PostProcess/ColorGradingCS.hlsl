#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Reference:
* - https://dev.epicgames.com/documentation/en-us/unreal-engine/color-grading-and-the-filmic-tonemapper-in-unreal-engine
* - https://en.wikipedia.org/wiki/ASC_CDL
*
* Unreal-style colour grading: four tonal ranges (global, shadows, midtones,
* highlights), each with saturation / contrast / gamma / gain / offset,
* applied in that order. The ordering is load-bearing - each stage works on
* the previous stage's output, so swapping two of them gives a different
* image.
*
* Contrast pivots around 0.18, scene-referred middle grey. That single
* constant is what fixes this pass's position in the frame: 0.18 only means
* "middle grey" once exposure has placed the scene there, and stops meaning
* anything at all once the tone curve has compressed the range. So grading
* has to sit after exposure and before the curve, exactly where Unreal puts
* it ("Scene Referred Linear Space" in their docs).
*
* Sitting there is also why this pass owns work that would otherwise belong
* to ToneMappingCS.hlsl: it composites the scene, adds the light
* contributions, and applies exposure, then grades, while ToneMappingCS.hlsl
* skips all of that and applies only the curve. That prologue is duplicated
* in both shaders rather than factored into a shared include on purpose -
* the tone mapping pass is the last thing standing between the frame and
* the screen, and giving it one more file that can fail to compile means a
* mistake anywhere in that file blacks out the whole image.
*
* Per-range wheels are COMBINED with the global ones rather than replacing
* them, so global stays a master move and each range is a relative
* adjustment on top. The three ranges are then blended by luminance with
* smooth crossovers - hard thresholds would draw a visible contour line
* through any smooth gradient the moment two ranges are graded differently.
*
* ---------------------------------------------------------------------
*
* [JP]
* Unreal 方式のカラーグレーディング: 4つの階調域(全体・シャドウ・中間調・
* ハイライト)それぞれに彩度/コントラスト/ガンマ/ゲイン/オフセットを持ち、
* その順に適用する。順序には意味があり、各段が前段の出力に対して働くので、
* 2つを入れ替えると違う絵になる。
*
* コントラストは 0.18(シーン参照の中間グレー)を軸に回す。この定数1つが
* このパスのフレーム内での位置を決めている: 0.18 が「中間グレー」を意味
* するのは露出がシーンをそこへ置いた後だけで、トーンカーブがレンジを
* 圧縮した後では何の意味も持たなくなる。よってグレーディングは露出の後・
* カーブの前に置くしかなく、それは Unreal が置いている位置
* (ドキュメントの言う "Scene Referred Linear Space")と同じ。
*
* そこに座ることの帰結として、このパスは本来 ToneMappingCS.hlsl の仕事で
* あるものも引き受ける: シーンの合成、光の加算寄与、露出の適用
* を行ってからグレーディングし、ToneMappingCS.hlsl はそれらを全て
* スキップしてカーブだけを適用する。この前半部分は共有インクルードに
* 切り出さず、両方のシェーダに重複して書いてある — これは意図的で、
* トーンマップパスは画面までの最後の砦であり、そこに「コンパイルに
* 失敗しうるファイル」を1つ増やすことは、そのファイルのどこかの
* ミスで画面全体が黒くなることを意味するから。
*
* 階調域ごとのホイールは全体の値を置き換えるのではなく【掛け合わせる】ので、
* 全体はマスターの動き、各域はその上に乗る相対的な調整という関係になる。
* 3つの域は輝度によってなだらかなクロスオーバーでブレンドする — 硬い
* しきい値にすると、2つの域を別々にグレーディングした瞬間、滑らかな
* グラデーションに等高線が見えてしまう。
*/

/**
* [EN]
* Builds the LMS cone-space adaptation coefficients from a color temperature.
* Derives the target white point's CIE xy chromaticity, converts it to LMS,
* and takes the ratio against D65.
*
* This is what makes it fundamentally different from "multiplying RGB by a
* color": a plain multiply just tints, whereas this is von Kries chromatic
* adaptation, which moves the WHITE POINT ITSELF - something that was white
* stays white, just under a different illuminant. This is exactly what a
* real camera's white balance does.
*
* The standard illuminant's y is a function of x (the quadratic below), an
* approximation of the blackbody locus.
*
* ---------------------------------------------------------------------
*
* [JP]
* 色温度から LMS 錐体空間での順応係数を作る。目標とする白色点の CIE xy 色度を
* 求め、それを LMS へ変換し、D65 との比を取る。
*
* これが「RGBに色を掛ける」のと決定的に違う点: 掛け算は色を塗るだけだが、
* こちらは【白色点そのもの】を動かす von Kries の色順応なので、白かったもの
* が白のまま別の照明下の白になる。実際のカメラのホワイトバランスがやって
* いるのはこちら。
*
* 標準光源の y は x の関数(下の2次式)で、黒体軌跡の近似。
*/
float3 WhiteBalanceCoefficients(float temperature)
{
	float shift = temperature * 1.5;

	float x = 0.31271 - shift * (shift < 0.0 ? 0.1 : 0.05);
	float y = 2.87 * x - 3.0 * x * x - 0.27509507;

	float capital_y = 1.0;
	float capital_x = capital_y * x / max(y, 0.0001);
	float capital_z = capital_y * (1.0 - x - y) / max(y, 0.0001);

	float3 target_lms;
	target_lms.x = 0.7328 * capital_x + 0.4296 * capital_y - 0.1624 * capital_z;
	target_lms.y = -0.7036 * capital_x + 1.6975 * capital_y + 0.0061 * capital_z;
	target_lms.z = 0.0030 * capital_x + 0.0136 * capital_y + 0.9834 * capital_z;

	/// [JP] D65(sRGBの基準白色点)の LMS 値。temperature が0のとき
	///      target_lms がこれと一致し、係数が全て1になって無変換になる。
	const float3 d65_lms = float3(0.949237, 1.03542, 1.08728);

	return d65_lms / max(target_lms, 0.0001);
}

/**
* [EN]
* Linear RGB -> LMS -> multiply by the adaptation coefficients -> linear RGB.
* Applying the adaptation in cone-response space (LMS) is the whole point of
* von Kries - doing the same thing in RGB space shifts hue.
*
* ---------------------------------------------------------------------
*
* [JP]
* リニアRGB → LMS → 順応係数を掛ける → リニアRGB。順応は錐体応答の空間
* (LMS)で行うのが von Kries の要点で、RGB空間で同じことをすると色相が
* ずれる。
*/
float3 ApplyWhiteBalance(float3 color, float temperature)
{
	const float3x3 linear_to_lms = float3x3(
		3.90405e-1, 5.49941e-1, 8.92632e-3,
		7.08416e-2, 9.63172e-1, 1.35775e-3,
		2.31082e-2, 1.28021e-1, 9.36245e-1);

	const float3x3 lms_to_linear = float3x3(
		2.85847e+0, -1.62879e+0, -2.48910e-2,
		-2.10182e-1, 1.15820e+0, 3.24281e-4,
		-4.18120e-2, -1.18169e-1, 1.06867e+0);

	float3 lms = mul(linear_to_lms, color);
	lms *= WhiteBalanceCoefficients(temperature);

	return mul(lms_to_linear, lms);
}

/**
* [EN]
* One tonal range's worth of correction. Order is white balance -> saturation
* -> contrast -> gamma -> gain -> offset. The five wheels follow the same
* order as Unreal's panel; white balance comes first because it is a
* photographic correction, not a creative one.
*
* Contrast divides by 0.18 before applying the power and multiplies it back
* afterward, to make middle grey the axis that stays fixed - without this,
* raising contrast alone would brighten (or darken) the whole image.
*
* Gamma is inverted to match photographic convention, so raising the value
* makes the image brighter.
*
* ---------------------------------------------------------------------
*
* [JP]
* 1つの階調域ぶんの補正。順序はホワイトバランス → 彩度 → コントラスト →
* ガンマ → ゲイン → オフセット。ホイール5つは Unreal のパネルと同じ順で、
* ホワイトバランスが先頭なのは創作的な操作ではなく撮影側の補正だから。
*
* コントラストが 0.18 で割ってから戻しているのは、中間グレーを動かさない
* 軸にするため - こうしないとコントラストを上げるだけで画面全体が明るく
* (または暗く)なってしまう。
*
* ガンマが逆数なのは写真側の慣習に合わせたもので、値を上げると明るくなる
* 向きになる。
*/
float3 ColorCorrect(float3 color, float temperature, float saturation, float contrast, float gamma, float gain, float offset)
{
	color = max(0.0, ApplyWhiteBalance(color, temperature));

	float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
	color = max(0.0, lerp(luminance.xxx, color, saturation));

	/// [EN] The exponent must always be confined to a finite range.
	///      Especially for gamma, which is inverted - a lower bound as loose
	///      as "anything but 0" (e.g. 0.0001) would let the exponent reach
	///      10000, crushing every color below 1 to 0 and turning the
	///      【entire screen black】. This is why lowering gamma close to 0
	///      alone blacks out the image. The range matches PostProcess.h's
	///      slider, 0.1-4.0. This clamps again rather than trusting the
	///      CPU-side value because a scene saved before that range changed
	///      can still carry an old out-of-range value.
	/// [JP] 指数は必ず有限の範囲に閉じ込める。特にガンマは逆数を取るので、
	///      下限を 0.0001 のような「0でなければ何でもいい」値にすると
	///      指数が 10000 になり、1未満の色が全て0へ潰れて【全画面が黒】に
	///      なる。ガンマを0付近まで下げただけで画面が落ちるのはこれ。
	///      範囲は PostProcess.h のスライダーと同じ 0.1〜4.0。CPU側の値を
	///      信用せずここでも掛けるのは、範囲を変更する前に保存された
	///      シーンが旧い値をそのまま持ってくるため。
	float safe_contrast = clamp(contrast, 0.1, 4.0);
	float safe_gamma = clamp(gamma, 0.1, 4.0);

	color = pow(max(color, 0.0) * (1.0 / 0.18), safe_contrast) * 0.18;
	color = pow(max(color, 0.0), 1.0 / safe_gamma);

	return color * gain + offset;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[constant_indices.post_process_.color_grading_.destination_uav_index_];

	uint width, height;
	destination.GetDimensions(width, height);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual screen edge
	///      must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際の画面端を超えたスレッドはどの
	///      リソースにも触れる前に抜ける必要がある。
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	/// [EN] Everything from here through exposure is the same processing as
	///      ToneMappingCS.hlsl's grading-disabled path. Not factored into a
	///      shared include - the display pass is the last line of defense,
	///      so it deliberately avoids adding a dependency that could drag
	///      it down if that other file fails to compile.
	/// [JP] ここから露出までは ToneMappingCS.hlsl のグレーディング無効時の
	///      経路と同じ処理。共有のインクルードには切り出していない -
	///      表示パスは最後の砦なので、他のファイルの都合であちらが道連れに
	///      コンパイル不能になる依存を増やしたくない。
	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);

	float3 color;
	if (constant_indices.post_process_.lens_stage_enabled_ != 0)
	{
		Texture2D<float4> lens_stage_source = ResourceDescriptorHeap[constant_indices.post_process_.lens_stage_srv_index_];
		color = lens_stage_source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	}
	else if (constant_indices.post_process_.depth_of_field_.enabled_ != 0)
	{
		Texture2D<float4> depth_of_field_source = ResourceDescriptorHeap[constant_indices.post_process_.depth_of_field_.shader_resource_view_index_];
		color = depth_of_field_source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	}
	else
	{
		Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.source_color_index_];
		color = source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	}

	if (constant_indices.post_process_.lens_flare_.enabled_ != 0)
	{
		Texture2D<float4> lens_flare = ResourceDescriptorHeap[constant_indices.post_process_.lens_flare_.shader_resource_view_index_];
		color += lens_flare.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	}

	if (constant_indices.post_process_.bloom_.enabled_ != 0)
	{
		Texture2D<float4> bloom = ResourceDescriptorHeap[constant_indices.post_process_.bloom_.level0_srv_index_];
		color += bloom.SampleLevel(sampler_linear_clamp, uv, 0).rgb * constant_indices.post_process_.bloom_.intensity_;
	}

	if (constant_indices.post_process_.anamorphic_flare_.enabled_ != 0)
	{
		Texture2D<float4> anamorphic_flare = ResourceDescriptorHeap[constant_indices.post_process_.anamorphic_flare_.output_srv_index_];
		color += anamorphic_flare.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	}

	float exposure_ev = constant_indices.post_process_.exposure_.exposure_compensation_;
	if (constant_indices.post_process_.exposure_.auto_exposure_enabled_ != 0)
	{
		RWStructuredBuffer<float> exposure = ResourceDescriptorHeap[constant_indices.post_process_.exposure_.exposure_uav_index_];
		exposure_ev += exposure[0];
	}

	color *= exp2(exposure_ev);

	ColorGradingRangeIndices global_range = constant_indices.post_process_.color_grading_.global_;
	ColorGradingRangeIndices shadows_range = constant_indices.post_process_.color_grading_.shadows_;
	ColorGradingRangeIndices midtones_range = constant_indices.post_process_.color_grading_.midtones_;
	ColorGradingRangeIndices highlights_range = constant_indices.post_process_.color_grading_.highlights_;

	/// [EN] Each range's value is 【multiplied】 with the global value
	///      (offset and temperature are added). This makes the global wheel
	///      the master move, with each range a relative adjustment on top.
	/// [JP] 階調域ごとの値は全体の値に【掛け合わせる】(オフセットと色温度は
	///      加算)。こうすることで全体がマスター、各域がその上の相対調整、
	///      という関係になる。
	float3 shadows_result = ColorCorrect(color,
		global_range.temperature_ + shadows_range.temperature_,
		global_range.saturation_ * shadows_range.saturation_,
		global_range.contrast_ * shadows_range.contrast_,
		global_range.gamma_ * shadows_range.gamma_,
		global_range.gain_ * shadows_range.gain_,
		global_range.offset_ + shadows_range.offset_);

	float3 midtones_result = ColorCorrect(color,
		global_range.temperature_ + midtones_range.temperature_,
		global_range.saturation_ * midtones_range.saturation_,
		global_range.contrast_ * midtones_range.contrast_,
		global_range.gamma_ * midtones_range.gamma_,
		global_range.gain_ * midtones_range.gain_,
		global_range.offset_ + midtones_range.offset_);

	float3 highlights_result = ColorCorrect(color,
		global_range.temperature_ + highlights_range.temperature_,
		global_range.saturation_ * highlights_range.saturation_,
		global_range.contrast_ * highlights_range.contrast_,
		global_range.gamma_ * highlights_range.gamma_,
		global_range.gain_ * highlights_range.gain_,
		global_range.offset_ + highlights_range.offset_);

	/// [EN] Which range a pixel belongs to is decided by luminance.
	///      Switching smoothly via smoothstep is the key point - a hard
	///      threshold would draw a visible contour through a sky or
	///      gradient the moment two ranges are graded differently. The
	///      midtones weight is "whatever remains", so the three weights
	///      always sum to 1.
	/// [JP] どの域に属するかは輝度で決める。smoothstep でなだらかに
	///      切り替えるのが要点で、硬いしきい値にすると2つの域を別々に
	///      グレーディングした瞬間、空やグラデーションに等高線が出る。
	///      中間調の重みは「残り」なので、3つの重みは必ず1になる。
	float shadows_max = constant_indices.post_process_.color_grading_.shadows_max_;
	float highlights_min = constant_indices.post_process_.color_grading_.highlights_min_;

	float graded_luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
	float shadows_weight = 1.0 - smoothstep(0.0, max(shadows_max, 0.0001), graded_luminance);
	float highlights_weight = smoothstep(highlights_min, max(highlights_min * 2.0, highlights_min + 0.0001), graded_luminance);
	float midtones_weight = saturate(1.0 - shadows_weight - highlights_weight);

	color = shadows_result * shadows_weight + midtones_result * midtones_weight + highlights_result * highlights_weight;

	destination[dtid.xy] = float4(max(color, 0.0), 1.0);
}
