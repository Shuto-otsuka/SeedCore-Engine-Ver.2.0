#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../Reflection/Reflection.hlsli"
#include "AmbientOcclusion.hlsli"

/**
* [EN]
* Stochastic ray-traced ambient occlusion pass (inline RayQuery). One thread
* per screen pixel, ONE cosine-weighted hemisphere ray per pixel per frame:
* reconstructs world position + normal from the G-Buffer, traces a short ray
* (ray_length_) and writes binary openness (1 = open, 0 = occluded). The 1spp
* noise is temporally accumulated by AmbientOcclusionDenoiseCS.hlsl (same
* reprojection + neighborhood-clamp scheme as the shadow denoiser), which
* converges the running average toward the true hemisphere occlusion ratio -
* so no multi-sample loop is needed here.
*
* [JP]
* 確率的レイトレAOパス(インライン RayQuery)。1スレッド=1ピクセル、
* 1フレームにつきコサイン重み付き半球レイを1本だけ:
* G-Buffer からワールド座標と法線を復元し、短いレイ(ray_length_)を飛ばして
* 開放度(1=開放 / 0=遮蔽)を二値で書く。1sppのノイズは
* AmbientOcclusionDenoiseCS.hlsl(影のデノイザと同じリプロジェクション+
* 近傍クランプ方式)が時間積分し、走り平均が真の半球遮蔽率へ収束する —
* だからここで複数サンプルのループは要らない。
*/
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	uint2 pixel = dtid.xy;
	RWTexture2D<float> raw_openness = ResourceDescriptorHeap[structured_indices.ambient_occlusion_.raw_uav_index_];

	// 背景(reverse-Z 遠平面=0)は遮蔽なし=1.0。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		raw_openness[pixel] = 1.0;
		return;
	}

	// ワールド座標と法線を復元(ShadowRT.hlsl と同じ手順、mul は行ベクトル左)。
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

	// 毎フレーム変わるRNGシード。同フレームの ShadowRT.hlsl と乱数列が
	// 揃わないよう frame_index_ に定数オフセットを混ぜる。
	uint rng_state = SeedFromPixel(pixel, tuning.frame_index_ + 7919);

	float3 ray_direction = CosineSampleHemisphere(rng_state, normal);

	// 面すれすれの方向のレイは、補間されたシェーディング法線とメッシュの実面
	// (ジオメトリ)のズレにより自分自身の隣接三角形へ刺さり、曲面(球など)に
	// 黒い斑点の誤遮蔽が出る。法線方向のバイアスに加えてレイ方向にも原点を
	// 押し出すことで、すれすれレイほど自分の面から離れて撃ち出されるようにする。
	RayDesc ray_desc;
	ray_desc.Origin = origin + ray_direction * tuning.normal_bias_;
	ray_desc.Direction = ray_direction;
	ray_desc.TMin = 0.001;
	ray_desc.TMax = tuning.ray_length_;

	raw_openness[pixel] = IsReflectionRayOccluded(tlas, ray_desc, structured_indices.raytracing_.instance_data_index_) ? 0.0 : 1.0;
}
