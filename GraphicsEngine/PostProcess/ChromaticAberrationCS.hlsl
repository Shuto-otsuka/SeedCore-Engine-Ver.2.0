#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Reference:
* - https://www.ncbi.nlm.nih.gov/books/NBK597386/
* - https://docs.unity3d.com/Packages/com.unity.postprocessing@3.4/manual/Chromatic-Aberration.html
*
* Lateral (transverse) chromatic aberration. A lens magnifies each
* wavelength slightly differently, so a single scene point images at a
* slightly different RADIUS per colour. That is the whole shape of the
* effect: zero fringing at the optical centre, growing toward the corners,
* and always oriented along the radial direction - which is why the samples
* below march along (uv - 0.5) rather than in a fixed screen direction.
*
* The other kind, LONGITUDINAL (axial) aberration, focuses wavelengths at
* different DEPTHS and appears as purple/green haloes on out-of-focus
* highlights uniformly across the frame. It is not modelled here: it needs
* per-wavelength focus and belongs with depth of field.
*
* Sampling only R/G/B at three offsets produces three visibly separated
* coloured copies rather than a fringe, so this marches sample_count_
* samples along the same radial line and weights each by a smooth spectral
* response. Unity's implementation does the same thing with a 3x1 spectral
* LUT texture; the response here is computed procedurally so no asset is
* needed. The accumulated weights are normalized per channel, which is what
* keeps the effect from tinting the image overall - without that, the
* procedural response would push a colour cast across the whole frame.
*
* ---------------------------------------------------------------------
*
* [JP]
* 倍率色収差(横方向)。レンズは波長ごとにわずかに異なる倍率で結像するため、
* シーン中の1点が色ごとに少しずつ違う【半径】に写る。効果の形はこれで
* 決まる: 光軸中心では色ずれが0で、四隅へ向かうほど大きくなり、常に半径
* 方向を向く — 下のサンプルが画面固定方向ではなく (uv - 0.5) 方向へ
* 刻んでいるのはそのため。
*
* もう一方の【軸上色収差(縦方向)】は波長ごとに合焦【距離】がずれるもので、
* ピントの外れたハイライトに画面全域で一様な紫/緑の縁として出る。ここでは
* 扱わない: 波長ごとの合焦が必要で、被写界深度の領分だから。
*
* R/G/B の3点だけをずらすと「3つの色の分身」が見えてしまい縁のにじみに
* ならないので、同じ半径方向の線上を sample_count_ 個のサンプルで刻み、
* それぞれに滑らかなスペクトル応答で重みを付ける。Unity の実装が 3x1 の
* スペクトルLUTテクスチャで同じことをしているが、ここでは手続き的に計算
* するのでアセットが要らない。累積した重みをチャンネルごとに正規化して
* いるのが、効果全体で色被りしない理由 — これをやらないと手続き的な応答
* そのものの色が画面全体に乗ってしまう。
*/

// [JP] サンプル位置 t(0..1)に対するスペクトル応答。位相をずらした
//      コサインで赤→緑→青へ滑らかに遷移させ、3タップのような離散的な
//      分身ではなく連続した虹色の縁にする。
float3 SpectralResponse(float t)
{
	return saturate(0.5 + 0.5 * cos(6.28318530718 * (t + float3(0.0, -0.33, -0.67))));
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[constant_indices.post_process_.chromatic_aberration_.destination_uav_index_];

	uint width, height;
	destination.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.chromatic_aberration_.source_srv_index_];

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);

	float intensity = constant_indices.post_process_.chromatic_aberration_.intensity_;
	uint sample_count = clamp(constant_indices.post_process_.chromatic_aberration_.sample_count_, 3u, 16u);

	/// [JP] 中心からのベクトルがそのまま色ずれの向きと量になる — 中心で0、
	///      四隅で最大。これが倍率色収差の定義そのもの。
	float2 offset_from_center = uv - 0.5;
	float2 maximum_offset = offset_from_center * intensity * 2.0;

	float3 accumulated_color = float3(0.0, 0.0, 0.0);
	float3 accumulated_weight = float3(0.0, 0.0, 0.0);

	[loop]
	for (uint index = 0; index < sample_count; index++)
	{
		float t = float(index) / float(sample_count - 1);
		float3 weight = SpectralResponse(t);

		/// [JP] t=0 が内側、t=1 が外側。長波長ほど外へずらすのが
		///      倍率色収差の向きで、SpectralResponse も t=0 側を赤に
		///      していないのはそのため(赤い重みは t が大きい側で立つ)。
		float2 sample_uv = uv + maximum_offset * t;

		accumulated_color += source.SampleLevel(sampler_linear_clamp, sample_uv, 0).rgb * weight;
		accumulated_weight += weight;
	}

	destination[dtid.xy] = float4(accumulated_color / max(accumulated_weight, 0.0001), 1.0);
}
