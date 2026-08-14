#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../Froxel/Froxel.hlsli"
#include "../VolumetricLight/VolumetricLight.hlsli"

/**
* [JP]
* Froxel ボリューメトリクス — パス3: 積分(compute、RT不使用)。パイプラインの
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

		// このスライスの厚み(指数分布スライスの隣接 view_z 差)。
		float z0 = FroxelSliceToViewZ(float(z) / float(froxel_dimensions.z), scene.near_plane_, scene.far_plane_);
		float z1 = FroxelSliceToViewZ(float(z + 1) / float(froxel_dimensions.z), scene.near_plane_, scene.far_plane_);
		float slice_depth = z1 - z0;

		float slice_transmittance = exp(-extinction * slice_depth);

		// エネルギー保存の解析積分(スライス内で散乱しつつ減衰する項)。
		float3 integrated = (in_scattering - in_scattering * slice_transmittance) / max(extinction, 0.00001);
		accumulated_scattering += transmittance * integrated;
		transmittance *= slice_transmittance;

		integration_volume[uint3(dtid.xy, z)] = float4(accumulated_scattering, transmittance);
	}
}
