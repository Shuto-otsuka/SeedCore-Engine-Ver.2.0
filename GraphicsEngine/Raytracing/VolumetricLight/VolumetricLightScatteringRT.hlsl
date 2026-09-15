#include "../../Shader/Scene.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/UnorderedAccesses.hlsli"
#include "../../Shader/Constants.hlsli"
#include "../../Light/Light.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../Froxel/Froxel.hlsli"
#include "../VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "../Reflection/Reflection.hlsli"
#include "../Shadow/Shadow.hlsli"
#include "VolumetricLight.hlsli"

static const float FROXEL_TEMPORAL_ALPHA = 0.1;

/**
* [EN]
* Reference:
* - https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
*   (Wronski, "Volumetric Fog: Unified Compute Shader Based Solution to
*   Atmospheric Scattering", SIGGRAPH 2014 - the froxel-grid technique this
*   whole 3-pass pipeline (FogInjectionCS / this file /
*   FroxelIntegrationCS.hlsl) implements, and the 1-sample temporal jitter +
*   reprojection it proposes against froxel undersampling.)
* - https://www.slideshare.net/slideshow/physically-based-and-unified-volumetric-rendering-in-frostbite/51840934
*   (Hillaire, "Physically Based and Unified Volumetric Rendering in
*   Frostbite", SIGGRAPH 2015 - temporal integration of the jittered
*   scattering and extinction with an exponential moving average.)
* - https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine
*   (Unreal Engine Volumetric Fog - a different sub-voxel jitter per frame
*   smoothed by a heavy temporal reprojection filter; history weight 0.9.)
* - https://www.mattiasstrand.com/posts/volumetric-fog/
*   (Strand, "Volumetric Fog" - reprojecting the froxel center into the
*   previous frame's clip space, including its depth slice.)
*
* Froxel volumetrics - pass 2: light scattering (inline RayQuery). The god
* ray / light shaft itself. Reads the density froxels pass 1 (FogInjection)
* wrote, and for a point inside every froxel (offset by a new jitter each
* frame), tests whether light reaches it from the directional light in two
* stages:
*   1. TLAS shadow ray, jittered within the sun disk - a spot shadowed by a
*      building or statue becomes a dark streak (a god ray's negative space).
*   2. (When the procedural cloud system is enabled) a short cloud
*      lightmarch - crepuscular rays shining through gaps in the clouds.
* then blends the phase-function-weighted in-scattering and the extinction
* with this view's previous frame, reprojected from the froxel center. RT is
* only used for the occlusion test, so inline RayQuery is the right fit.
* Dispatch: froxel_dimensions / [4,4,4].
*
* ---------------------------------------------------------------------
*
* [JP]
* Froxel ボリューメトリクス - パス2: 光の散乱(インライン RayQuery)。
* God Ray / 光芒の本体。パス1(FogInjection)が書いた密度 froxel を読み、
* 各 froxel の内部の点(毎フレーム違うジッターでずらす)からディレクショナル
* ライトへ:
*   1. 太陽円盤内でジッターした TLAS シャドウレイ - 建物や像に遮られた場所は
*      暗い筋になる(ゴッドレイ)
*   2. (プロシージャル雲有効時)短い雲ライトマーチ - 雲間から光芒が差す
* の2段で「光が届くか」を判定し、位相関数ぶんの内散乱と消衰を、froxel 中心
* から再投影したこのビューの前フレームとブレンドして書き込む。
* RT を使うのは遮蔽判定だけなのでインライン RayQuery が最適。
* Dispatch: froxel_dimensions / [4,4,4]。
*/
[numthreads(4, 4, 4)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	ConstantBuffer<VolumetricLightRayConstantBuffer> tuning = ResourceDescriptorHeap[constant_indices.volumetric_light_index_];
	uint3 froxel_dimensions = uint3(tuning.froxel_dimension_x_, tuning.froxel_dimension_y_, tuning.froxel_dimension_z_);

	/// [EN] Bounds guard: the froxel grid's dimensions are not necessarily a
	///      multiple of the [4,4,4] thread group size, so any axis can
	///      overshoot.
	/// [JP] 範囲外ガード: froxel グリッドの次元は [4,4,4] スレッドグループ
	///      サイズの倍数とは限らないので、どの軸でもはみ出し得る。
	if (any(dtid >= froxel_dimensions))
	{
		return;
	}

	ConstantBuffer<VolumetricLightDispatchConstantBuffer> dispatch = GetVolumetricLightDispatchConstantBuffer();

	RWTexture3D<float4> density_volume = ResourceDescriptorHeap[unordered_access_indices.volumetric_light_.density_index_];
	RWTexture3D<float4> scattering_volume = ResourceDescriptorHeap[dispatch.scattering_write_unordered_access_view_index_];

	float4 medium = density_volume[dtid];
	float3 scattering_coefficient = medium.rgb;
	float extinction = medium.a;

	/// [EN] A froxel with no medium has zero scattering (extinction still
	///      carries through unchanged).
	/// [JP] 媒質が無い froxel は散乱ゼロ(消衰だけ持ち越す)。
	if (all(scattering_coefficient <= 0.0))
	{
		scattering_volume[dtid] = float4(0, 0, 0, extinction);
		return;
	}

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	float3 world_position = FroxelToWorldExact(float3(dtid) + 0.5 + dispatch.jitter_, froxel_dimensions, scene.near_plane_, scene.far_plane_, scene.projection_, scene.inverse_view_);
	float3 view_direction = normalize(world_position - scene.camera_position_.xyz);

	ConstantBuffer<LightConstantBuffer> light = ResourceDescriptorHeap[constant_indices.light_index_];
	ConstantBuffer<VolumetricCloudScapesRayConstantBuffer> cloud_tuning = ResourceDescriptorHeap[constant_indices.cloud_index_];

	/// [EN] Ambient (sky) in-scattering. Without this, distant fog collapses
	///      to "sun term near-zero x transmittance near-zero = pure black".
	///      Real fog isotropically scatters the WHOLE sky's light, so
	///      distant fog converges to a "sky-colored haze" (equilibrium =
	///      albedo x the sky's average radiance).
	/// [JP] 環境光(空)の内散乱。これが無いと遠景の霧は「太陽項ほぼゼロ×
	///      透過率ほぼゼロ=真っ黒」に潰れる。本物の霧は空全体の光を等方的に
	///      散乱するので、遠くは「空色のもや」へ収束する(equilibrium =
	///      アルベド×空の平均放射輝度)。
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

	if (GetDirectionalLightConstantBuffer().sun_intensity_ > 0.0)
	{
		float3 light_direction = normalize(-GetDirectionalLightConstantBuffer().direction_);
		float visibility = 1.0;

		/// [EN] 1. Occlusion by scene geometry (skipped - treated as
		///      unoccluded - on a frame with no TLAS yet).
		/// [JP] 1. シーンジオメトリによる遮蔽(TLAS が無いフレームは
		///      スキップ=非遮蔽)。
		if (shader_resource_indices.raytracing_.tlas_index_ != 0xFFFFFFFF)
		{
			RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[shader_resource_indices.raytracing_.tlas_index_];

			ConstantBuffer<ShadowRayConstantBuffer> shadow_tuning = ResourceDescriptorHeap[constant_indices.shadow_index_];
			uint rng_state = SeedFromPixel(dtid.xy, dispatch.frame_index_ + dtid.z * 7919u);

			RayDesc ray_desc;
			ray_desc.Origin = world_position;
			ray_desc.Direction = SampleCone(rng_state, light_direction, cos(shadow_tuning.sun_angular_radius_));
			ray_desc.TMin = 0.001;
			ray_desc.TMax = tuning.ray_t_max_;

			if (IsReflectionRayOccluded(tlas, ray_desc, shader_resource_indices.raytracing_.instance_data_index_))
			{
				visibility = 0.0;
			}
		}

		/// [EN] 2. Dimming by clouds (crepuscular rays through cloud gaps).
		///      A 3-step cloud lightmarch toward the sun.
		/// [JP] 2. 雲による減光(雲間からの光芒)。太陽方向へ3ステップの
		///      雲ライトマーチ。
		if (visibility > 0.0 && tuning.cloud_shadow_enabled_ != 0 && cloud_tuning.procedural_sky_enabled_ != 0 && light_direction.y > 0.001)
		{
			Texture3D<float> shape_noise = ResourceDescriptorHeap[shader_resource_indices.cloud_.shape_noise_index_];
			Texture3D<float> detail_noise = ResourceDescriptorHeap[shader_resource_indices.cloud_.detail_noise_index_];

			/// [EN] Find the segment where the ray from this froxel toward
			///      the sun enters the cloud layer, and sample 3 points
			///      within it. The intersection uses the same spherical-
			///      shell solver as VolumetricCloudScapesRT.hlsl - solving
			///      with a flat plane here instead would put the light
			///      shaft out of alignment with where the clouds are
			///      actually drawn.
			/// [JP] froxel から太陽方向に雲層へ入る区間を求め、層内を3点
			///      サンプルする。交差は VolumetricCloudScapesRT.hlsl と
			///      同じ球殻ソルバを使う - ここだけ平板で解くと、光芒が
			///      実際に描かれている雲とずれる。
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
			float3 sun_radiance = GetDirectionalLightConstantBuffer().sun_color_.rgb * GetDirectionalLightConstantBuffer().sun_intensity_;

			in_scattering += visibility * scattering_coefficient * phase * sun_radiance * tuning.godray_strength_;
		}
	}

	float4 scattering = float4(in_scattering, extinction);

	if (dispatch.history_valid_ != 0)
	{
		float3 center_position = FroxelToWorldExact(float3(dtid) + 0.5, froxel_dimensions, scene.near_plane_, scene.far_plane_, scene.projection_, scene.inverse_view_);
		float4 previous_clip = mul(float4(center_position, 1.0), scene.previous_non_jitter_view_projection_);

		if (previous_clip.w > 0.0)
		{
			float2 previous_ndc = previous_clip.xy / previous_clip.w;
			float3 previous_uvw = float3(previous_ndc.x * 0.5 + 0.5, 0.5 - previous_ndc.y * 0.5, ViewZToFroxelSlice(previous_clip.w, scene.near_plane_, scene.far_plane_));

			if (all(previous_uvw >= 0.0) && all(previous_uvw <= 1.0))
			{
				Texture3D<float4> scattering_history = ResourceDescriptorHeap[dispatch.scattering_history_shader_resource_view_index_];
				scattering = lerp(scattering_history.SampleLevel(sampler_linear_clamp, previous_uvw, 0), scattering, FROXEL_TEMPORAL_ALPHA);
			}
		}
	}

	scattering_volume[dtid] = scattering;
}
