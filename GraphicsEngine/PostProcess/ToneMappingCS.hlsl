#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"
#include "ToneMappingCurves.hlsli"

/**
* [EN]
* The actual display pass: reads the HDR scene color, applies exposure (the
* ExposureSettings.compensation_ EV always, plus the auto-exposure buffer's
* EV when enabled), applies the selected tone curve (or none - clamp only),
* and encodes to sRGB before writing into the UNORM texture the editor/game
* viewport actually shows. Runs unconditionally every frame regardless of
* ToneMappingSettings.enabled_/ExposureSettings.enabled_ - what those toggle
* is whether the CURVE and the AUTO EV apply, not whether this pass runs,
* because the sRGB encode below is not optional: the swapchain backbuffer is
* DXGI_FORMAT_R8G8B8A8_UNORM (no hardware sRGB view anywhere in this engine),
* so this is the only place a linear HDR value ever gets gamma-corrected for
* display. Before this pass existed the raw linear buffer was shown directly,
* which is why scenes without a bright emissive/sun disc always read as
* crushed and dark - see the exposure comment in PostProcess.h for the actual
* numbers.
*
* [JP]
* 実際の表示パス: HDR シーンカラーを読み、露出(ExposureSettings.compensation_
* の EV は常に、自動露出バッファの EV は有効時に加算)を適用し、選択された
* トーンカーブ(またはカーブ無し=クランプのみ)を適用し、エディタ/ゲーム
* ビューポートが実際に表示する UNORM テクスチャへ書く前に sRGB エンコードする。
* ToneMappingSettings.enabled_ / ExposureSettings.enabled_ に関わらず毎フレーム
* 無条件で走る — それらが切り替えるのは【カーブ】と【自動EV】の適用有無で
* あって、このパス自体の実行有無ではない。下の sRGB エンコードは省略できない
* からだ: スワップチェーンのバックバッファは DXGI_FORMAT_R8G8B8A8_UNORM
* (このエンジンにはハードウェア sRGB ビューがどこにも無い)なので、リニアな
* HDR 値がガンマ補正される場所はここが唯一。このパスが無かった頃は生のリニア
* バッファがそのまま表示されており、それが明るいエミッシブや太陽円盤の無い
* シーンが常に潰れて暗く見えていた理由 — 実数値は PostProcess.h の露出コメント
* 参照。
*/

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> output = ResourceDescriptorHeap[constant_indices.post_process_.output_uav_index_];

	uint width, height;
	output.GetDimensions(width, height);

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

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);

	/// [EN] When color grading is enabled, ColorGradingCS.hlsl has already
	///      finished scene compositing, additive light contributions and
	///      exposure, writing the graded result into its own buffer -
	///      grading has to sit "after exposure, before the tone curve" for
	///      its 0.18 contrast pivot to be meaningful, and sitting there means
	///      it also has to own every step up to that point. This pass just
	///      reads that and applies the curve. When disabled, everything is
	///      done here as before.
	///
	///      The same sequence of steps is duplicated in ColorGradingCS.hlsl
	///      rather than factored into a shared include - this display pass
	///      is the last line of defense, so it deliberately avoids a
	///      dependency that could drag it down if some other file fails to
	///      compile.
	/// [JP] カラーグレーディングが有効なときは、シーンの合成・光の加算寄与・
	///      露出まで ColorGradingCS.hlsl が済ませてグレーディング済みの
	///      バッファへ書いている。グレーディングは 0.18 のコントラスト軸の
	///      都合で「露出の後・トーンカーブの前」に居なければならず、その
	///      位置に居る以上そこまでの工程も持つことになるため。ここはそれを
	///      読んでカーブだけを掛ける。無効なときは従来どおりここで全部やる。
	///
	///      同じ工程が ColorGradingCS.hlsl 側にも書かれているが、共有の
	///      インクルードには切り出していない - 表示パスは最後の砦なので、
	///      他のファイルの都合でここが道連れにコンパイル不能になる依存を
	///      増やしたくない。
	float3 color;
	if (constant_indices.post_process_.color_grading_.enabled_ != 0)
	{
		Texture2D<float4> graded_source = ResourceDescriptorHeap[constant_indices.post_process_.color_grading_.output_srv_index_];
		color = graded_source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	}
	else
	{
		/// [EN] If the lens stage (distortion/chromatic aberration/vignette)
		///      ran, the scene sits in its shared buffer, so that is read
		///      first. The chain of branches is already resolved CPU-side by
		///      PrepareView, so this only needs one check. The lens stage's
		///      and depth of field's buffers are always at native
		///      resolution, so they can disagree with output when DLSS-RR is
		///      enabled - which is why this reads via UV-based SampleLevel
		///      instead of Load.
		/// [JP] レンズ段(歪曲/色収差/ビネット)が走った場合、シーンはその
		///      共有バッファに置かれているので最優先で読む。分岐の連鎖は
		///      PrepareView がCPU側で解決済みなので、ここは1回の判定で済む。
		///      レンズ段と被写界深度のバッファは常にネイティブ解像度なので、
		///      DLSS-RR有効時に output と食い違う。Load ではなく UV 経由の
		///      SampleLevel で読むのはそのため。
		float3 hdr_color;
		if (constant_indices.post_process_.lens_stage_enabled_ != 0)
		{
			Texture2D<float4> lens_stage_source = ResourceDescriptorHeap[constant_indices.post_process_.lens_stage_srv_index_];
			hdr_color = lens_stage_source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
		}
		else if (constant_indices.post_process_.depth_of_field_.enabled_ != 0)
		{
			Texture2D<float4> depth_of_field_source = ResourceDescriptorHeap[constant_indices.post_process_.depth_of_field_.shader_resource_view_index_];
			hdr_color = depth_of_field_source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
		}
		else
		{
			Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.source_color_index_];
			hdr_color = source.Load(int3(dtid.xy, 0)).rgb;
		}

		/// [EN] The "additive light" effect group. All are added before
		///      exposure - they ARE light, and light is what gets exposed.
		///      Each is skipped entirely by sample while disabled, since its
		///      buffer would otherwise still hold stale data from the last
		///      time it was enabled.
		/// [JP] 光を「足す」エフェクト群。いずれも露出より前に加算する -
		///      これらは光であり、光は露出されるものだから。無効な間は
		///      バッファに前回有効だった時のデータが残っているので、
		///      サンプルごと丸ごとスキップする。
		if (constant_indices.post_process_.lens_flare_.enabled_ != 0)
		{
			Texture2D<float4> lens_flare = ResourceDescriptorHeap[constant_indices.post_process_.lens_flare_.shader_resource_view_index_];
			hdr_color += lens_flare.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
		}

		if (constant_indices.post_process_.bloom_.enabled_ != 0)
		{
			Texture2D<float4> bloom = ResourceDescriptorHeap[constant_indices.post_process_.bloom_.level0_srv_index_];
			hdr_color += bloom.SampleLevel(sampler_linear_clamp, uv, 0).rgb * constant_indices.post_process_.bloom_.intensity_;
		}

		if (constant_indices.post_process_.anamorphic_flare_.enabled_ != 0)
		{
			Texture2D<float4> anamorphic_flare = ResourceDescriptorHeap[constant_indices.post_process_.anamorphic_flare_.output_srv_index_];
			hdr_color += anamorphic_flare.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
		}

		/// [EN] The manual EV always applies. The auto-exposure EV is only
		///      added on top when enabled (see PostProcess.h's
		///      ExposureSettings).
		/// [JP] 手動EVは常に効く。自動露出のEVは有効時だけ上乗せする
		///      (PostProcess.h の ExposureSettings 参照)。
		float exposure_ev = constant_indices.post_process_.exposure_.exposure_compensation_;
		if (constant_indices.post_process_.exposure_.auto_exposure_enabled_ != 0)
		{
			RWStructuredBuffer<float> exposure = ResourceDescriptorHeap[constant_indices.post_process_.exposure_.exposure_uav_index_];
			exposure_ev += exposure[0];
		}

		color = hdr_color * exp2(exposure_ev);
	}

	if (constant_indices.post_process_.tone_mapping_.tone_mapping_enabled_ != 0)
	{
		uint mode = constant_indices.post_process_.tone_mapping_.tone_mapping_mode_;
		if (mode == 1)
		{
			color = ReinhardToneMap(color);
		}
		else if (mode == 2)
		{
			color = AcesFilmicToneMap(color);
		}
		else if (mode == 3)
		{
			color = PbrNeutralToneMap(color);
		}
		/// [EN] mode == 0 (None): no curve, only the saturate below applies.
		/// [JP] mode == 0 (None): カーブ無し、下の saturate だけがかかる。
	}

	color = saturate(color);
	color = LinearToSrgb(color);

	output[dtid.xy] = float4(color, 1.0);
}
