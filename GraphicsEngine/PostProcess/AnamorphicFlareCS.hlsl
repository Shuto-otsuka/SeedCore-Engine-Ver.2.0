#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Reference:
* - https://bartwronski.com/2015/03/09/anamorphic-lens-flares-and-visual-effects/
* - https://github.com/keijiro/KinoStreak
* - https://www.chrisoat.com/papers/Oat-ScenePostprocessing.pdf
*
* Anamorphic flare: the long horizontal streak a cine anamorphic lens throws
* off bright points. Separate from LensFlareCS.hlsl because the cause is
* different - that one is aperture diffraction plus inter-element
* reflections, this one is the cylindrical squeeze element.
*
* The streak is horizontal for a non-obvious reason, and getting the order
* right is what makes this implementation simple. An anamorphic lens
* SQUEEZES the image 2:1 horizontally while shooting. The internal
* reflection happens inside that squeezed space as an ordinary ROUND flare.
* Projection then stretches the image back out, and the round flare becomes
* a horizontal streak. So the passes here work in a horizontally squeezed
* buffer (half the width of the other quarter-res post-process buffers) and
* Compose samples it back with normal UVs - the 2x horizontal stretch falls
* out of the sampling for free, with no directional logic needed for it.
*
* A 2:1 squeeze on its own only yields a 2:1 ellipse though, and real
* anamorphic streaks are far longer than that. So BlurPass1..4 additionally
* blur horizontally inside the squeezed space, out to streak_length_. That
* part is art direction rather than physics and it is not energy conserving
* - every shipped implementation of this effect does the same (KinoStreak
* says as much about itself).
*
* The blur itself is Kawase's light streak, the same kernel LensFlareCS.hlsl
* uses: 3 taps per side at a texel spacing of 4^(pass-1), a tap `d` texels
* out weighted attenuation^d, normalized by the weight sum. The exponential
* decay IS the streak's falloff - an equal-weight box average produces a bar
* of constant brightness instead of light.
*
* ---------------------------------------------------------------------
*
* [JP]
* アナモルフィックフレア: シネマ用アナモルフィックレンズが明るい点に対して
* 出す、横方向の長い筋。LensFlareCS.hlsl と分けているのは原因が別だから —
* あちらは絞りの回折と素子間反射、こちらはシリンドリカル(円柱)圧縮素子。
*
* 筋が横向きになる理由は直感に反し、その順序を正しく捉えることがこの実装を
* 単純にしている。アナモルフィックレンズは撮影時に像を水平方向へ【2:1に
* 圧縮】する。レンズ内部反射はその圧縮空間の中で【普通の丸いフレア】として
* 起きる。上映時に水平へ引き伸ばして戻すことで、丸かったフレアが横長の筋に
* なる。したがってここのパス群は横に圧縮されたバッファ(他の1/4解像度
* ポストプロセスバッファの半分の幅)の中で処理し、Compose が通常のUVで
* サンプルして戻す — 横2倍の引き伸ばしはサンプル時に勝手に起きるので、
* そのための方向処理は一切要らない。
*
* ただし 2:1 の圧縮だけでは 2:1 の楕円にしかならず、実写のアナモルフィック
* 筋はそれよりはるかに細長い。そこで BlurPass1..4 が圧縮空間の中でさらに
* 横方向へ streak_length_ ぶんブラーして長さを稼ぐ。この部分は物理では
* なくアートディレクションで、エネルギー保存もしていない — このエフェクトの
* 実装は軒並みそうしている(KinoStreak は自らそう明言している)。
*
* ブラー自体は Kawase のライトストリークで、LensFlareCS.hlsl と同じカーネル:
* 片側3タップを 4^(パス-1) テクセル間隔で取り、d テクセル先のタップの重みを
* attenuation^d にし、重みの合計で正規化する。この指数減衰が【筋の減衰
* そのもの】で、等重みの箱平均にすると明るさが一定のバーになって光に
* 見えない。
*/

float3 AnamorphicBrightPass(float3 color, float threshold)
{
	float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
	float weight = max(luminance - threshold, 0.0) / max(luminance, 0.0001);
	return color * weight;
}

// [JP] Kawase のライトストリークのカーネル(横方向固定)。中心1タップ +
//      左右3タップずつ、重みは attenuation^(テクセル距離)。
//
//      減衰は必ず1未満に抑える。1を超えると pow が減衰ではなく指数
//      【増加】になり、テクセル距離が最大 64*3 = 192 まで伸びるため
//      すぐ Inf になる。すると最後の sum / weight_sum が Inf/Inf = NaN を
//      返し、NaN が Compose から出力バッファへ、さらに ToneMappingCS.hlsl の
//      hdr_color への加算で画面全体へ広がって真っ黒になる。CPU側の
//      スライダー範囲を信用してはいけない — レンジ変更前に保存された
//      シーンには旧レンジの値が入っており、レンズフレア側で実際に
//      これで黒画面になった。
//
//      重み合計で正規化するのも必須で、しないと1パスあたり最大7倍の
//      ゲインが掛かって4パスで飽和する。明るさを決めるのはカーネルの
//      ゲインではなく intensity_。
float3 AnamorphicStreakGather(Texture2D<float4> source, float2 uv, float texel_width, float spacing, float attenuation, float length_scale)
{
	float safe_attenuation = clamp(attenuation, 0.5, 0.999);

	float3 sum = source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	float weight_sum = 1.0;

	[unroll]
	for (uint tap = 1; tap <= 3; tap++)
	{
		float texel_distance = spacing * float(tap);
		float weight = pow(safe_attenuation, texel_distance);
		float offset = texel_width * texel_distance * length_scale;

		sum += source.SampleLevel(sampler_linear_clamp, uv + float2(offset, 0.0), 0).rgb * weight;
		sum += source.SampleLevel(sampler_linear_clamp, uv - float2(offset, 0.0), 0).rgb * weight;
		weight_sum += weight * 2.0;
	}

	return sum / weight_sum;
}

// [JP] BlurPass1..4 の共通本体。read_ping が true なら ping を読んで pong へ、
//      false なら pong を読んで ping へ書く(パスごとにコンパイル時固定)。
void AnamorphicBlurPass(uint3 dtid, float spacing, bool read_ping)
{
	uint source_index = read_ping ? constant_indices.post_process_.anamorphic_flare_.ping_srv_index_ : constant_indices.post_process_.anamorphic_flare_.pong_srv_index_;
	uint destination_index = read_ping ? constant_indices.post_process_.anamorphic_flare_.pong_uav_index_ : constant_indices.post_process_.anamorphic_flare_.ping_uav_index_;

	RWTexture2D<float4> destination = ResourceDescriptorHeap[destination_index];

	uint width, height;
	destination.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[source_index];

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float texel_width = 1.0 / float(width);
	float attenuation = constant_indices.post_process_.anamorphic_flare_.attenuation_;
	float length_scale = constant_indices.post_process_.anamorphic_flare_.streak_length_ * 4.0;

	destination[dtid.xy] = float4(AnamorphicStreakGather(source, uv, texel_width, spacing, attenuation, length_scale), 1.0);
}

// [JP] Prefilter: フル解像度のHDRソース(DOF有効時はDOF出力)をしきい値抽出
//      しつつ圧縮バッファへ落とす。圧縮バッファは横が1/8・縦が1/4なので、
//      1画素が担当するソース範囲は【8x4】。バイリニアタップ1つが2x2を
//      平均するので、横4タップ x 縦2タップでその範囲を丸ごと覆う。
//      ここを手抜きして疎にサンプルすると、太陽のような数画素幅の光源を
//      読み飛ばして筋が一切出なくなる(レンズフレア側で実際に踏んだ罠)。
//      しきい値は平均の【前】にタップごとに掛ける — 後に掛けると小さな
//      光源が平均で薄まった後にしきい値で消える。
[numthreads(8, 8, 1)]
void Prefilter(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[constant_indices.post_process_.anamorphic_flare_.ping_uav_index_];

	uint width, height;
	destination.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	uint source_index = constant_indices.post_process_.depth_of_field_.enabled_ != 0 ? constant_indices.post_process_.depth_of_field_.shader_resource_view_index_ : constant_indices.post_process_.source_color_index_;
	Texture2D<float4> source = ResourceDescriptorHeap[source_index];

	uint source_width, source_height;
	source.GetDimensions(source_width, source_height);
	float2 source_texel = 1.0 / float2(source_width, source_height);

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float threshold = constant_indices.post_process_.anamorphic_flare_.threshold_;

	float3 result = float3(0.0, 0.0, 0.0);

	[unroll]
	for (uint row = 0; row < 2; row++)
	{
		float vertical_offset = (float(row) * 2.0 - 1.0) * source_texel.y;

		[unroll]
		for (uint column = 0; column < 4; column++)
		{
			float horizontal_offset = (float(column) * 2.0 - 3.0) * source_texel.x;
			float2 tap_uv = uv + float2(horizontal_offset, vertical_offset);

			result += AnamorphicBrightPass(source.SampleLevel(sampler_linear_clamp, tap_uv, 0).rgb, threshold);
		}
	}

	destination[dtid.xy] = float4(result * 0.125, 1.0);
}

// [JP] パスごとのテクセル間隔は 4^(パス番号-1) = 1, 4, 16, 64。Kawase の
//      ライトストリークが定める倍率で、片側3タップなので到達距離は
//      3 + 12 + 48 + 192 = 255 テクセル。圧縮バッファは横がネイティブの
//      1/8 しかないため、これは画面の横幅を優に超える長さになる —
//      アナモルフィックの筋が画面を横切るのはこのため。
[numthreads(8, 8, 1)]
void BlurPass1(uint3 dtid : SV_DispatchThreadID)
{
	AnamorphicBlurPass(dtid, 1.0, true);
}

[numthreads(8, 8, 1)]
void BlurPass2(uint3 dtid : SV_DispatchThreadID)
{
	AnamorphicBlurPass(dtid, 4.0, false);
}

[numthreads(8, 8, 1)]
void BlurPass3(uint3 dtid : SV_DispatchThreadID)
{
	AnamorphicBlurPass(dtid, 16.0, true);
}

[numthreads(8, 8, 1)]
void BlurPass4(uint3 dtid : SV_DispatchThreadID)
{
	AnamorphicBlurPass(dtid, 64.0, false);
}

// [JP] Compose: 圧縮バッファを【通常のUV】でサンプルする。出力は圧縮
//      されていないので、ここで横方向に2倍引き伸ばされる — これが
//      「丸いフレアが横長の筋になる」上映時の引き伸ばしそのもの。
//      その上で tint_(レンズ個体の特性であって物理定数ではない、
//      PostProcess.h のコメント参照)と intensity_ を掛けて書き出す。
//
//      パス4は pong を読んで ping へ書くので、最終結果は ping にある。
[numthreads(8, 8, 1)]
void Compose(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> output = ResourceDescriptorHeap[constant_indices.post_process_.anamorphic_flare_.output_uav_index_];

	uint width, height;
	output.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.anamorphic_flare_.ping_srv_index_];

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float intensity = constant_indices.post_process_.anamorphic_flare_.intensity_;
	float3 tint = constant_indices.post_process_.anamorphic_flare_.tint_.rgb;

	float3 streak = source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;

	output[dtid.xy] = float4(streak * tint * intensity, 1.0);
}
