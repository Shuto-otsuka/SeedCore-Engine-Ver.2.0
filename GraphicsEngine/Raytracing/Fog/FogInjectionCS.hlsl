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
*   whole 3-pass pipeline (this file / VolumetricLightScatteringRT.hlsl /
*   FroxelIntegrationCS.hlsl) implements.)
*
* Froxel volumetrics - pass 1: fog medium injection (compute, no raytracing).
* Writes "scattering coefficient (rgb)" and "extinction coefficient (a)" into
* every cell of the view-frustum froxel grid. This IS the medium's
* definition: base density x height fog (exponential falloff). Pass 2
* (VolumetricLightScatteringRT) reads this density to compute light
* scattering. Dispatch: froxel_dimensions / [4,4,4]. One thread = one froxel.
*
* ---------------------------------------------------------------------
*
* [JP]
* Froxel ボリューメトリクス - パス1: フォグ媒質の注入(compute、RT不使用)。
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

	/// [EN] Bounds guard: unlike a screen-space dispatch, the froxel grid's
	///      dimensions are not necessarily a multiple of the [4,4,4] thread
	///      group size, so any axis can overshoot.
	/// [JP] 範囲外ガード: 画面空間のディスパッチと違い、froxel グリッドの
	///      次元は [4,4,4] スレッドグループサイズの倍数とは限らないので、
	///      どの軸でもはみ出し得る。
	if (any(dtid >= froxel_dimensions))
	{
		return;
	}

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	RWTexture3D<float4> density_volume = ResourceDescriptorHeap[structured_indices.volumetric_light_.density_uav_index_];

	float3 world_position = FroxelToWorldExact(dtid, froxel_dimensions, scene.near_plane_, scene.far_plane_, scene.projection_, scene.inverse_view_);

	/// [EN] Height fog: exponential falloff above the reference height
	///      (falloff 0 gives uniform fog).
	/// [JP] 高さフォグ: 基準高さから上に行くほど指数減衰(falloff 0 で
	///      一様フォグ)。
	float density = tuning.density_ * exp(-tuning.height_falloff_ * max(world_position.y - tuning.height_reference_, 0.0));

	/// [EN] Scattering + absorption = extinction (Beer-Lambert).
	/// [JP] 散乱 + 吸収 = 消衰(Beer-Lambert)。
	float3 scattering = density * tuning.fog_albedo_;
	float extinction = density + tuning.absorption_;

	density_volume[dtid] = float4(scattering, extinction);
}
