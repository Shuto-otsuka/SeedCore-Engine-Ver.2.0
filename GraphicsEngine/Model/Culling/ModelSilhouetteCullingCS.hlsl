#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Culling.hlsli"

[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	SceneConstantBuffer scene = GetSceneConstantBuffer();

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
				float world_radius = bound.radius_ * world_scale;

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
		ModelCullingConstantBuffer culling = GetModelCullingConstantBuffer();
		RWByteAddressBuffer arguments = ResourceDescriptorHeap[culling.arguments_index_];

		ModelCullingStructuredBuffer visible;
		visible.instance_index_ = instance_id;
		visible.meshlet_index_ = instance.geometry_.meshlet_offset_ + meshlet_local;
		visible.shell_index_ = 0;
		visible.model_culling_structured_buffer_padding_0_ = 0;

		uint slot;
		if (instance.shading_.double_sided_ == 0)
		{
			arguments.InterlockedAdd(4, 1, slot);
			RWStructuredBuffer<ModelCullingStructuredBuffer> single_sided = ResourceDescriptorHeap[culling.single_sided_index_];
			single_sided[slot] = visible;
		}
		else
		{
			arguments.InterlockedAdd(20, 1, slot);
			RWStructuredBuffer<ModelCullingStructuredBuffer> double_sided = ResourceDescriptorHeap[culling.double_sided_index_];
			double_sided[slot] = visible;
		}
	}
}
