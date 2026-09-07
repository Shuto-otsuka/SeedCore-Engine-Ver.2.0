#include "../Model/Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"

float4 main(ModelMSOutput input) : SV_Target0
{
	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance instance = instances[input.instance_index];
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	float2 screen_uv = input.position.xy * scene.inverse_screen_size_;
	float4 ndc_position = float4(screen_uv.x * 2.0 - 1.0, 1.0 - screen_uv.y * 2.0, input.position.z, 1.0);
	float4 world_homogeneous = mul(ndc_position, scene.inverse_view_projection_);
	float3 world_position = world_homogeneous.xyz / world_homogeneous.w;

	float3 face_normal = normalize(cross(ddx(world_position), ddy(world_position)));
	float3 light_direction = normalize(scene.camera_position_.xyz - world_position);
	if (dot(face_normal, light_direction) < 0.0)
	{
		face_normal = -face_normal;
	}

	float diffuse = saturate(dot(face_normal, light_direction));
	float3 sky_color = float3(0.55, 0.60, 0.68);
	float3 ground_color = float3(0.20, 0.18, 0.17);
	float3 ambient = lerp(ground_color, sky_color, face_normal.y * 0.5 + 0.5);

	float3 base_color = instance.base_color_.rgb;
	float3 lit_color = base_color * (ambient + diffuse * 0.9);

	return float4(lit_color, 1.0);
}
