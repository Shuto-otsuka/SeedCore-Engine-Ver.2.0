#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../Reflection/Reflection.hlsli"
#include "AmbientOcclusion.hlsli"

/**
* [EN]
* Distance to the closest occluder along ray_desc, or a negative value when the
* ray reaches ray_desc.TMax unoccluded. Same alpha handling as
* IsReflectionRayOccluded (Reflection.hlsli), but keeps the hit distance, which
* the openness falloff below needs.
*
* ---------------------------------------------------------------------
*
* [JP]
* ray_desc に沿った最も近い遮蔽物までの距離。TMax まで遮蔽が無ければ負値を
* 返す。アルファの扱いは IsReflectionRayOccluded(Reflection.hlsli)と同じだが、
* 下の開放度の減衰に必要なヒット距離を保持する。
*/
float AmbientOcclusionOccluderDistance(RaytracingAccelerationStructure tlas, RayDesc ray_desc, uint instance_data_index)
{
	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray_desc);

	while (query.Proceed())
	{
		if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
		{
			if (!IsReflectionMaterialPassthrough(instance_data_index, query.CandidateInstanceID(), query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics()))
			{
				query.CommitNonOpaqueTriangleHit();
			}
		}
	}

	return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? query.CommittedRayT() : -1.0;
}

/**
* [EN]
* Reference:
* - http://www.realtimerendering.com/raytracinggems/rtg/index.html
*   (Ray Tracing Gems, Chapter 15 "On the Importance of Sampling" - cosine-
*   weighted hemisphere sampling for RTAO, ~30% lower error than uniform.)
* - https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.1/manual/Ray-Traced-Ambient-Occlusion.html
*   (Unity HDRP's RTAO - the distance-based falloff this pass mirrors,
*   instead of a hard binary cutoff at ray_length_.)
*
* Stochastic ray-traced ambient occlusion pass (inline RayQuery). One thread
* per screen pixel, ONE cosine-weighted hemisphere ray per pixel per frame:
* reconstructs world position + normal from the G-Buffer, traces a short ray
* (ray_length_) and writes openness falling off with occluder distance (1 =
* open/no occluder within range, 0 = occluder essentially at the surface).
* The 1spp noise is temporally accumulated by AmbientOcclusionDenoiseCS.hlsl
* (same reprojection + neighborhood-clamp scheme as the shadow denoiser),
* which converges the running average toward the true hemisphere occlusion
* ratio - so no multi-sample loop is needed here.
*
* ---------------------------------------------------------------------
*
* [JP]
* 確率的レイトレAOパス(インライン RayQuery)。1スレッド=1ピクセル、
* 1フレームにつきコサイン重み付き半球レイを1本だけ:
* G-Buffer からワールド座標と法線を復元し、短いレイ(ray_length_)を飛ばして
* 開放度を「遮蔽物までの距離で減衰する連続値」として書く(1=射程内に
* 遮蔽物なし、0=遮蔽物がほぼ面上)。1sppのノイズは
* AmbientOcclusionDenoiseCS.hlsl(影のデノイザと同じリプロジェクション+
* 近傍クランプ方式)が時間積分し、走り平均が真の半球遮蔽率へ収束する —
* だからここで複数サンプルのループは要らない。
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
	RWTexture2D<float> raw_openness = ResourceDescriptorHeap[structured_indices.ambient_occlusion_.raw_uav_index_];

	/// [EN] Background (reverse-Z far plane = 0) is unoccluded = 1.0 - there
	///      is no surface here for anything to occlude.
	/// [JP] 背景(reverse-Z 遠平面=0)は遮蔽なし=1.0。ここには遮蔽され得る
	///      面が無い。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		raw_openness[pixel] = 1.0;
		return;
	}

	/// [EN] Reconstruct world position and normal (same procedure as
	///      ShadowRT.hlsl, mul takes the row vector on the left).
	/// [JP] ワールド座標と法線を復元する(ShadowRT.hlsl と同じ手順、mul は
	///      行ベクトル左)。
	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 world_position = world.xyz / world.w;

	ConstantBuffer<AmbientOcclusionRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.ambient_occlusion_.ray_constant_index_];

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);
	float3 origin = world_position + normal * tuning.normal_bias_;

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

	/// [EN] RNG seed that changes every frame. A constant offset is mixed
	///      into frame_index_ so this pass's random sequence does not line
	///      up with ShadowRT.hlsl's in the same frame (lining up would
	///      correlate the two passes' noise into visible banding).
	/// [JP] 毎フレーム変わる RNG シード。同フレームの ShadowRT.hlsl と
	///      乱数列が揃わないよう frame_index_ に定数オフセットを混ぜる
	///      (揃うとノイズが相関して縞に見える)。
	uint rng_state = SeedFromPixel(pixel, tuning.frame_index_ + 7919);

	float3 ray_direction = CosineSampleHemisphere(rng_state, normal);

	/// [EN] A ray at a near-grazing angle can, due to the mismatch between
	///      the interpolated shading normal and the mesh's actual geometric
	///      face, stab into its own neighboring triangle - showing up as
	///      false black occlusion spots on curved surfaces (spheres, for
	///      example). On top of the normal-direction bias, also push the
	///      origin along the RAY direction itself, so the more grazing a
	///      ray is, the farther it starts from its own surface.
	/// [JP] 面すれすれの方向のレイは、補間されたシェーディング法線と
	///      メッシュの実面(ジオメトリ)のズレにより自分自身の隣接三角形へ
	///      刺さり、曲面(球など)に黒い斑点の誤遮蔽が出る。法線方向の
	///      バイアスに加えてレイ方向にも原点を押し出すことで、すれすれ
	///      レイほど自分の面から離れて撃ち出されるようにする。
	RayDesc ray_desc;
	ray_desc.Origin = origin + ray_direction * tuning.normal_bias_;
	ray_desc.Direction = ray_direction;
	ray_desc.TMin = 0.001;
	ray_desc.TMax = tuning.ray_length_;

	/// [EN] Fall off with occluder distance rather than a binary hit/miss.
	///      A binary result cuts off hard at ray_length_'s boundary, so
	///      changing the AO radius makes shadows near walls visibly step.
	///      A farther occluder blocks less of the incoming ambient light, so
	///      its contribution is faded linearly with distance (the standard
	///      RTAO treatment - the same falloff Unity HDRP and NVIDIA NRD's AO
	///      input both use).
	/// [JP] 二値ではなく遮蔽物までの距離で減衰させる。二値だと ray_length_
	///      の境界で遮蔽が硬く切れ、AO 半径を変えた時に壁際の陰が段差として
	///      動く。遠い遮蔽物ほど届く環境光を遮らないので、寄与を距離で
	///      線形に落とす(RTAO の定番 - Unity HDRP の falloff や NRD の AO
	///      入力と同じ扱い)。
	float occluder_distance = AmbientOcclusionOccluderDistance(tlas, ray_desc, structured_indices.raytracing_.instance_data_index_);

	raw_openness[pixel] = occluder_distance < 0.0 ? 1.0 : saturate(occluder_distance / max(tuning.ray_length_, 0.0001));
}
