#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../Froxel/Froxel.hlsli"
#include "../VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "VolumetricLight.hlsli"

/**
* [JP]
* Froxel ボリューメトリクス — パス2: 光の散乱(インライン RayQuery)。
* God Ray / 光芒の本体。パス1(FogInjection)が書いた密度 froxel を読み、
* 各 froxel の中心からディレクショナルライトへ:
*   1. TLAS シャドウレイ — 建物や像に遮られた場所は暗い筋になる(ゴッドレイ)
*   2. (プロシージャル雲有効時)短い雲ライトマーチ — 雲間から光芒が差す
* の2段で「光が届くか」を判定し、位相関数ぶんの内散乱を書き込む。
* RT を使うのは遮蔽判定だけなのでインライン RayQuery が最適。
* Dispatch: froxel_dimensions / [4,4,4]。
*/
[numthreads(4, 4, 4)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	ConstantBuffer<VolumetricLightRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.volumetric_light_.ray_constant_index_];
	uint3 froxel_dimensions = uint3(tuning.froxel_dimension_x_, tuning.froxel_dimension_y_, tuning.froxel_dimension_z_);

	if (any(dtid >= froxel_dimensions))
	{
		return;
	}

	RWTexture3D<float4> density_volume = ResourceDescriptorHeap[structured_indices.volumetric_light_.density_uav_index_];
	RWTexture3D<float4> scattering_volume = ResourceDescriptorHeap[structured_indices.volumetric_light_.scattering_uav_index_];

	float4 medium = density_volume[dtid];
	float3 scattering_coefficient = medium.rgb;
	float extinction = medium.a;

	// 媒質が無い froxel は散乱ゼロ(消衰だけ持ち越す)。
	if (all(scattering_coefficient <= 0.0))
	{
		scattering_volume[dtid] = float4(0, 0, 0, extinction);
		return;
	}

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	float3 world_position = FroxelToWorldExact(dtid, froxel_dimensions, scene.near_plane_, scene.far_plane_, scene.projection_, scene.inverse_view_);
	float3 view_direction = normalize(world_position - scene.camera_position_.xyz);

	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];
	ConstantBuffer<VolumetricCloudScapesRayConstantBuffer> cloud_tuning = ResourceDescriptorHeap[structured_indices.cloud_.ray_constant_index_];

	// 環境光(空)の内散乱。これが無いと遠景の霧は「太陽項ほぼゼロ×透過率
	// ほぼゼロ=真っ黒」に潰れる。本物の霧は空全体の光を等方的に散乱するので、
	// 遠くは「空色のもや」へ収束する(equilibrium = アルベド×空の平均放射輝度)。
	float3 ambient_radiance;
	if (cloud_tuning.procedural_sky_enabled_ != 0)
	{
		ambient_radiance = lerp(cloud_tuning.sky_horizon_color_, cloud_tuning.sky_zenith_color_, 0.5) * cloud_tuning.sky_brightness_;
	}
	else
	{
		ambient_radiance = float3(0.4, 0.4, 0.4);
	}

	float3 in_scattering = scattering_coefficient * ambient_radiance;

	if (light.directional_intensity_ > 0.0)
	{
		float3 light_direction = normalize(-light.directional_direction_);
		float visibility = 1.0;

		// 1. シーンジオメトリによる遮蔽(TLAS が無いフレームはスキップ=非遮蔽)。
		if (structured_indices.raytracing_.tlas_index_ != 0xFFFFFFFF)
		{
			RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

			RayDesc ray_desc;
			ray_desc.Origin = world_position;
			ray_desc.Direction = light_direction;
			ray_desc.TMin = 0.001;
			ray_desc.TMax = tuning.ray_t_max_;

			RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
			query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray_desc);
			query.Proceed();

			if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
			{
				visibility = 0.0;
			}
		}

		// 2. 雲による減光(雲間からの光芒)。太陽方向へ3ステップの雲ライトマーチ。
		if (visibility > 0.0 && tuning.cloud_shadow_enabled_ != 0 && cloud_tuning.procedural_sky_enabled_ != 0 && light_direction.y > 0.001)
		{
			Texture3D<float> shape_noise = ResourceDescriptorHeap[structured_indices.cloud_.shape_noise_srv_index_];
			Texture3D<float> detail_noise = ResourceDescriptorHeap[structured_indices.cloud_.detail_noise_srv_index_];

			// froxel から太陽方向に雲層へ入る区間を求め、層内を3点サンプルする。
			// 交差は VolumetricCloudScapesRT.hlsl と同じ球殻ソルバを使う —
			// ここだけ平板で解くと、光芒が実際に描かれている雲とずれる。
			float t_enter;
			float t_exit;
			if (CloudLayerInterval(world_position, light_direction, cloud_tuning, t_enter, t_exit))
			{
				const uint cloud_step_count = 3;
				float cloud_curvature = CloudCurvature(light_direction, cloud_tuning.planet_radius_);
				float cloud_step_length = (t_exit - t_enter) / float(cloud_step_count);
				float optical_depth = 0.0;

				for (uint cloud_step = 0; cloud_step < cloud_step_count; cloud_step++)
				{
					float cloud_t = t_enter + cloud_step_length * (float(cloud_step) + 0.5);
					float cloud_altitude = CloudAltitudeAt(world_position.y, light_direction.y, cloud_curvature, cloud_t);
					float3 cloud_sample = world_position + light_direction * cloud_t;
					optical_depth += CloudDensity(cloud_sample, cloud_altitude, cloud_tuning, scene.total_time_, shape_noise, detail_noise, sampler_linear_wrap, 0.0) * cloud_step_length;
				}

				visibility *= exp(-optical_depth);
			}
		}

		if (visibility > 0.0)
		{
			float cos_theta = dot(view_direction, light_direction);
			float phase = PhaseHenyeyGreenstein(cos_theta, tuning.scattering_g_);
			float3 sun_radiance = light.directional_color_.rgb * light.directional_intensity_;

			in_scattering += visibility * scattering_coefficient * phase * sun_radiance * tuning.godray_strength_;
		}
	}

	scattering_volume[dtid] = float4(in_scattering, extinction);
}
