#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../../Shader/Denoiser.hlsli"
#include "Reflection.hlsli"
#include "ReflectionReSTIR.hlsli"

/**
* [EN]
* Reference:
* - https://cs.dartmouth.edu/~wjarosz/publications/bitterli20spatiotemporal.html
*   (Bitterli et al., "Spatiotemporal reservoir resampling for real-time ray
*   tracing with dynamic direct lighting", SIGGRAPH 2020 - the spatial reuse
*   pass this file implements, in the streaming-RIS form
*   ReflectionReSTIR.hlsli's Combine already uses.)
*
* ReSTIR spatial reuse for reflection: runs after ReflectionRayGeneration has
* written this frame's temporally-combined reservoir for every pixel (and the
* barrier that makes it SRV-readable) - reads the current pixel's own
* reservoir plus a few random neighbors, all from THIS frame's write slot
* (constant_indices.reflection_.reservoir_write_srv_index_), and streams them
* together the same way ReflectionRayGeneration folds in temporal history.
* Reading the neighbors from this frame's own data (instead of last frame's,
* as an in-raygen version of this pass would have to) is what makes the reuse
* correct - see GlobalIlluminationReservoirSpatialCS.hlsl's comment for the
* same reasoning, which this mirrors exactly.
*
* One thing this adds beyond the GI version: a roughness-similarity gate on
* top of the depth/normal one. A specular lobe narrows as roughness drops, so
* a neighbor's traced direction is a progressively worse stand-in for what
* THIS pixel would have traced as roughness diverges between them (a mirror
* borrowing a rough neighbor's sample would visibly blur; the reverse would
* import a spurious sharp highlight) - the RIS weighting alone does not
* capture this, since its target function is plain luminance, not
* GGX-lobe-aware.
*
* Writes the resolved radiance/hit-distance into the same raw texture
* ReflectionRayGeneration used to write directly
* (structured_indices.reflection_.output_uav_index_) - everything downstream
* (ReflectionDenoiseCS.hlsl's dual-reprojection SVGF, or DLSS Ray
* Reconstruction) is unaffected by this pass existing.
*
* ---------------------------------------------------------------------
*
* [JP]
* 反射用の ReSTIR 空間的リユース。ReflectionRayGeneration が全画素分の今
* フレームの時間的結合済み Reservoir を書き終え、SRV として読めるバリアが
* 済んだ後に走る - 自分のピクセルと近傍数点の Reservoir を、全て今フレームの
* 書き込みスロット(constant_indices.reflection_.reservoir_write_srv_index_)
* から読み、ReflectionRayGeneration が時間的履歴を畳み込むのと同じ要領で
* ストリーミング結合する。近傍を(raygen 内でやる場合のように)前フレームの
* データからではなく今フレーム自身のデータから読むのが正しさの要 —
* GlobalIlluminationReservoirSpatialCS.hlsl のコメントと同じ理由で、
* そのままここにも当てはまる。
*
* GI 版に対してここで追加しているのがラフネス類似度ゲート(深度/法線ゲートに
* 加えて)。スペキュラローブは roughness が下がるほど狭くなるため、両者の
* roughness が離れるほど近傍がトレースした方向は「このピクセルが自分で
* トレースしていたら」の代役として不適切になる(鏡面が粗い近傍のサンプルを
* 借りればぼやけて見え、逆はスペキュラハイライトが偽って映り込む) — RIS の
* 重み付けだけではこれを捉えられない(そのターゲット関数は単純な輝度であって
* GGX ローブを意識したものではないため)。
*
* 解決した放射輝度/ヒット距離は、ReflectionRayGeneration が直接書いていたのと
* 同じ生テクスチャ(structured_indices.reflection_.output_uav_index_)へ書く —
* 後段(ReflectionDenoiseCS.hlsl の二重リプロジェクション SVGF、または
* DLSS Ray Reconstruction)はこのパスの有無を意識しない。
*/
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual screen edge
	///      must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際の画面端を超えたスレッドはどの
	///      リソースにも触れる前に抜ける必要がある。
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	uint2 pixel = dtid.xy;

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	RWTexture2D<float4> output = ResourceDescriptorHeap[structured_indices.reflection_.output_uav_index_];

	if (depth == 0.0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	ConstantBuffer<ReflectionRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.reflection_.ray_constant_index_];

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float4 gbuffer1 = normal_texture.Load(int3(pixel, 0));
	float3 normal = OctNormalDecode(gbuffer1.rg);
	float roughness = gbuffer1.b;

	StructuredBuffer<ReflectionReservoir> reservoir_buffer = ResourceDescriptorHeap[constant_indices.reflection_.reservoir_write_srv_index_];
	ReflectionReservoir reservoir = reservoir_buffer[pixel.y * (uint)scene.screen_size_.x + pixel.x];

	/// [EN] Mix in a different constant offset from raygen's, so this pass's
	///      random sequence does not correlate with the temporal reuse
	///      pass's (correlated noise shows up as visible banding).
	/// [JP] raygen とは違う定数オフセットを混ぜ、時間的リユースの乱数列と
	///      相関しないようにする(相関するとノイズが縞に見える)。
	uint rng_state = SeedFromPixel(pixel, tuning.frame_index_ + 3266489917u);

	int2 screen_max = int2(scene.screen_size_) - 1;
	float2 depth_gradient = DenoiserDepthGradient(depth_texture, int2(pixel), screen_max);

	/// [EN] Makes up for the extra noise from a lowered
	///      REFLECTION_RESERVOIR_M_CAP (shorter effective temporal history)
	///      with spatial sample count instead of temporal - does not affect
	///      responsiveness, since this all runs within the same frame.
	/// [JP] REFLECTION_RESERVOIR_M_CAP を下げた(時間方向の実効履歴を短く
	///      した)ぶんのノイズ増を、時間方向ではなく空間方向のサンプル数で
	///      埋め合わせる - 応答速度には影響しない(同一フレーム内の処理の
	///      ため)。
	const uint SPATIAL_SAMPLE_COUNT = 8;
	const float SPATIAL_RADIUS = 16.0;
	const float SPATIAL_WEIGHT_THRESHOLD = 0.1;
	const float SPATIAL_DEPTH_SHARPNESS = 48.0;
	const float SPATIAL_NORMAL_POWER = 8.0;

	/// [EN] Tolerance for the roughness difference from the neighbor.
	///      Stricter near mirror-like (roughness->0), looser toward rough
	///      (roughness->1) - a rough surface already has a wide lobe, so it
	///      can absorb more directional mismatch from a neighbor.
	/// [JP] ラフネス差の許容量。鏡面(roughness->0)ほど厳しく、粗い面
	///      (roughness->1)ほど緩くする - 粗い面はもともとローブが広く、
	///      近傍の方向のずれを吸収できる。
	float roughness_tolerance = lerp(0.02, 0.3, roughness);

	[unroll]
	for (uint sampleIndex = 0; sampleIndex < SPATIAL_SAMPLE_COUNT; sampleIndex++)
	{
		float angle = Rand(rng_state) * 6.28318530718;
		float radius = sqrt(Rand(rng_state)) * SPATIAL_RADIUS;
		int2 neighborOffset = int2(round(float2(cos(angle), sin(angle)) * radius));
		int2 neighborPixel = clamp(int2(pixel) + neighborOffset, int2(0, 0), screen_max);

		float neighborDepth = depth_texture.Load(int3(neighborPixel, 0));
		float4 neighborGbuffer1 = normal_texture.Load(int3(neighborPixel, 0));
		float3 neighborNormal = OctNormalDecode(neighborGbuffer1.rg);
		float neighborRoughness = neighborGbuffer1.b;

		float spatialWeight = DenoiserSpatialWeight(depth, depth_gradient, neighborOffset, neighborDepth, normal, neighborNormal, SPATIAL_DEPTH_SHARPNESS, SPATIAL_NORMAL_POWER);

		bool neighborValid = neighborDepth > 0.0 && spatialWeight > SPATIAL_WEIGHT_THRESHOLD && abs(neighborRoughness - roughness) <= roughness_tolerance;

		uint neighborIndex = (uint)neighborPixel.y * (uint)scene.screen_size_.x + (uint)neighborPixel.x;
		ReflectionReservoir neighbor = reservoir_buffer[neighborIndex];

		reservoir = ReflectionReservoirCombine(reservoir, neighbor, neighborValid, rng_state);
	}

	output[pixel] = float4(reservoir.sample_radiance_ * reservoir.sample_w_, reservoir.sample_hit_distance_);

	/// [EN] Passes how converged this pixel's reservoir ultimately is
	///      (M / cap, after both temporal and spatial combine) to
	///      ReflectionDenoiseCS.hlsl. While M stays low right after a
	///      disocclusion, the denoiser's own temporal blend is left to do
	///      the work; once M has built up near the cap at steady state, the
	///      denoiser's blend is nearly bypassed - avoiding a double
	///      integration where the reservoir and SVGF EACH independently
	///      average over a long history (the felt cause of a "dragging"
	///      motion trail).
	/// [JP] このピクセルの reservoir が最終的にどれだけ収束しているか(時間的
	///      +空間的結合を経た後の M / 上限)を ReflectionDenoiseCS.hlsl へ
	///      渡す。ディスオクルージョン直後で M が低い間はデノイザ自身の
	///      時間的ブレンドに任せ、M が上限付近まで積み上がった定常状態では
	///      デノイザ側のブレンドをほぼバイパスする - reservoir と SVGF が
	///      【それぞれ独立に】長い時間平均を重ねる二重積分(体感的な
	///      「引きずられる」動きの原因)を避けるため。
	RWTexture2D<float> confidence_output = ResourceDescriptorHeap[structured_indices.reflection_.confidence_uav_index_];
	confidence_output[pixel] = saturate(reservoir.sample_m_ / REFLECTION_RESERVOIR_M_CAP);
}
