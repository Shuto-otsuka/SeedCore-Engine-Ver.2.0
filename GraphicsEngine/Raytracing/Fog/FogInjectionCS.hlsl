#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../Froxel/Froxel.hlsli"
#include "../VolumetricLight/VolumetricLight.hlsli"

/**
* [JP]
* Froxel ボリューメトリクス — パス1: フォグ媒質の注入(compute、RT不使用)。
* 視錐台 froxel グリッドの各セルへ「散乱係数(rgb)」と「消衰係数(a)」を書く。
* ここが媒質の定義そのもの: ベース密度×高さフォグ(指数減衰)。
* パス2(VolumetricLightScatteringRT)がこの密度を読んで光の散乱を計算する。
* Dispatch: froxel_dimensions / [4,4,4]。1スレッド=1 froxel。
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

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	RWTexture3D<float4> density_volume = ResourceDescriptorHeap[structured_indices.volumetric_light_.density_uav_index_];

	float3 world_position = FroxelToWorldExact(dtid, froxel_dimensions, scene.near_plane_, scene.far_plane_, scene.projection_, scene.inverse_view_);

	// 高さフォグ: 基準高さから上に行くほど指数減衰(falloff 0 で一様フォグ)。
	float density = tuning.density_ * exp(-tuning.height_falloff_ * max(world_position.y - tuning.height_reference_, 0.0));

	// 散乱 + 吸収 = 消衰(Beer-Lambert)。
	float3 scattering = density * tuning.fog_albedo_;
	float extinction = density + tuning.absorption_;

	density_volume[dtid] = float4(scattering, extinction);
}
