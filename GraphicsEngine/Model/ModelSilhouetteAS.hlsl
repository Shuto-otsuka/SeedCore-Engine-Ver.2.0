#include "Model.hlsli"
#include "../Shader/ShaderResources.hlsli"
#include "../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared ModelASPayload payload;

/**
* [JP]
* シルエット用 Amplification Shader。ModelAS.hlsl のコピー
* （編集時は同期を保つこと）に、selected_ != 0 のインスタンスだけを通す
* フィルタを追加したもの。エディタで選択中のアクターのみマスクに描かれる。
* Hi-Z オクルージョンカリングは意図的に行わない: 手前の未選択オブジェクトに
* 遮蔽されたメッシュレットもここでは間引かず、選択メッシュのシルエット全体を
* マスクへ描く（PSO 側も深度テストなし）。そうしないと、エッジ検出合成が
* 遮蔽物の輪郭までアウトラインとして拾ってしまう。
*/
[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID)
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	uint instance_id = gid.x;
	ModelStructuredBuffer instance = instances[instance_id];
	StructuredBuffer<ModelMeshletBound> bounds = ResourceDescriptorHeap[instance.geometry_.meshlet_bound_buffer_index_];

	uint meshlet_local = gtid.x;
	bool is_visible = false;

	if (instance.shading_.selected_ != 0 && meshlet_local < instance.geometry_.meshlet_count_)
	{
		float world_scale = max(max(length(instance.transform_.world_[0].xyz), length(instance.transform_.world_[1].xyz)), length(instance.transform_.world_[2].xyz));
		if (IsLodSelected(instance.streaming_.lod_error_, instance.streaming_.lod_error_next_, instance.transform_.world_[3].xyz, world_scale, scene.camera_position_.xyz, scene.projection_._m11, scene.screen_size_.y, 1.0))
		{
		if (instance.skining_.skin_index_ != 0xFFFFFFFF)
		{
			is_visible = true;
		}
		else
		{
		uint meshlet_global = instance.geometry_.meshlet_offset_ + meshlet_local;
		ModelMeshletBound bound = bounds[meshlet_global];

		float3 world_center = mul(float4(bound.center_, 1.0), instance.transform_.world_).xyz;
		float world_radius = bound.radius_ * max(max(length(instance.transform_.world_[0].xyz), length(instance.transform_.world_[1].xyz)), length(instance.transform_.world_[2].xyz));

		is_visible = IsVisibleInFrustum(world_center, world_radius, scene.current_view_projection_);

		if (is_visible && instance.shading_.double_sided_ == 0 && bound.cone_cutoff_ > 0.0)
		{
			float3 world_cone_axis = normalize(mul(float4(bound.cone_axis_, 0.0), instance.transform_.world_).xyz);
			float3 camera_to_center = world_center - scene.camera_position_.xyz;
			float cone_sine = sqrt(1.0 - bound.cone_cutoff_ * bound.cone_cutoff_);
			if (dot(camera_to_center, world_cone_axis) >= cone_sine * length(camera_to_center) + world_radius)
			{
				is_visible = false;
			}
		}
		}
		}
	}

	if (is_visible)
	{
		uint slot;
		InterlockedAdd(survived_count, 1, slot);
		local_indices[slot] = instance.geometry_.meshlet_offset_ + meshlet_local;
	}

	GroupMemoryBarrierWithGroupSync();

	if (gtid.x == 0)
	{
		payload.instance_index = instance_id;
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.meshlet_indices[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
