#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../Froxel/Froxel.hlsli"
#include "../VolumetricLight/VolumetricLight.hlsli"

/**
* [EN]
* Reference:
* - https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
*   (Wronski, "Volumetric Fog: Unified Compute Shader Based Solution to
*   Atmospheric Scattering", SIGGRAPH 2014 - the froxel-grid technique this
*   whole 3-pass pipeline (FogInjectionCS.hlsl / VolumetricLightScatteringRT.hlsl
*   / this file) implements, including the front-to-back analytic
*   scattering/transmittance integration this file performs per slice.)
*
* Froxel volumetrics - pass 3: integration (compute, no raytracing). The
* pipeline's exit point. Integrates the scattering froxels pass 2 wrote along
* depth (near -> far), writing "accumulated scattering up to here (rgb)" and
* "transmittance (a)" into every froxel. The composite
* (DeferredLightingPS.hlsl) samples this integrated volume by uv + depth
* slice and applies it onto the scene. One thread handles one XY column,
* sweeping Z from 0 to dim.z front-to-back exactly once. Dispatch:
* (dim.x/8, dim.y/8, 1).
*
* ---------------------------------------------------------------------
*
* [JP]
* Froxel ボリューメトリクス - パス3: 積分(compute、RT不使用)。パイプラインの
* 出口。パス2が書いた散乱 froxel を奥行き方向(手前→奥)に積分し、各 froxel へ
* 「そこまでの累積散乱(rgb)」と「透過率(a)」を書く。合成
* (DeferredLightingPS.hlsl)はこの積分ボリュームを uv+深度スライスで
* サンプルしてシーンに乗せる。
* 各 XY 列を1スレッドが担当し、Z を 0→dim.z へ front-to-back で1回舐める。
* Dispatch: (dim.x/8, dim.y/8, 1)。
*/
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	ConstantBuffer<VolumetricLightRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.volumetric_light_.ray_constant_index_];
	uint3 froxel_dimensions = uint3(tuning.froxel_dimension_x_, tuning.froxel_dimension_y_, tuning.froxel_dimension_z_);

	/// [EN] Bounds guard for the XY column this thread owns - Z is walked
	///      entirely within the loop below, so it needs no separate check.
	/// [JP] このスレッドが担当する XY 列の範囲外ガード - Z はこの下の
	///      ループ内で全域を歩くので個別のチェックは不要。
	if (dtid.x >= froxel_dimensions.x || dtid.y >= froxel_dimensions.y)
	{
		return;
	}

	SceneConstantBuffer scene = GetSceneConstantBuffer();

	RWTexture3D<float4> scattering_volume = ResourceDescriptorHeap[structured_indices.volumetric_light_.scattering_uav_index_];
	RWTexture3D<float4> integration_volume = ResourceDescriptorHeap[structured_indices.volumetric_light_.integration_uav_index_];

	float3 accumulated_scattering = float3(0, 0, 0);
	float transmittance = 1.0;

	for (uint z = 0; z < froxel_dimensions.z; z++)
	{
		float4 cell = scattering_volume[uint3(dtid.xy, z)];
		float3 in_scattering = cell.rgb;
		float extinction = cell.a;

		/// [EN] This slice's thickness (the gap in view_z between adjacent
		///      exponentially-spaced slices).
		/// [JP] このスライスの厚み(指数分布スライスの隣接 view_z 差)。
		float z0 = FroxelSliceToViewZ(float(z) / float(froxel_dimensions.z), scene.near_plane_, scene.far_plane_);
		float z1 = FroxelSliceToViewZ(float(z + 1) / float(froxel_dimensions.z), scene.near_plane_, scene.far_plane_);
		float slice_depth = z1 - z0;

		float slice_transmittance = exp(-extinction * slice_depth);

		/// [EN] Energy-conserving analytic integral (the term for light that
		///      scatters in while also attenuating within the slice).
		/// [JP] エネルギー保存の解析積分(スライス内で散乱しつつ減衰する項)。
		float3 integrated = (in_scattering - in_scattering * slice_transmittance) / max(extinction, 0.00001);
		accumulated_scattering += transmittance * integrated;
		transmittance *= slice_transmittance;

		integration_volume[uint3(dtid.xy, z)] = float4(accumulated_scattering, transmittance);
	}
}
