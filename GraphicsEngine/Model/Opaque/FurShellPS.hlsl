#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Constants.hlsli"
#include "../../Shader/Scene.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Light/Light.hlsli"

// Pixel shader for the shell-fur forward pass. Alpha-blended over the lit
// opaque frame. Per-strand hash noise decides how tall each strand is; a
// fragment on shell layer `shell_ratio` is kept only where a strand reaches
// that high. Shading is Kajiya-Kay fur against the scene directional light
// plus a cheap hemispheric ambient.

float hash21(float2 p)
{
	p = frac(p * float2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return frac(p.x * p.y);
}

float4 main(FurShellMSOutput input) : SV_Target0
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	ModelStructuredBuffer instance = instances[input.instance_index];

	// per-strand height from a hashed grid over UV; density packs more strands in.
	float2 strand_grid = floor(input.texcoord * (128.0 + instance.shading_.fur_density_ * 384.0));
	float strand_height = hash21(strand_grid);
	strand_height = strand_height * strand_height;           // bias towards shorter strands

	if (strand_height < input.shell_ratio)
	{
		discard;
	}

	float3 normal = normalize(input.world_normal);
	float3 fibre_tangent = normalize(input.world_tangent);

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	float3 view = normalize(scene.camera_position_.xyz - input.world_position);

	ConstantBuffer<LightConstantBuffer> light_constant = ResourceDescriptorHeap[constant_indices.light_index_];
	float3 light_direction = normalize(-GetDirectionalLightConstantBuffer().direction_);
	float3 light_color = GetDirectionalLightConstantBuffer().sun_color_.rgb * GetDirectionalLightConstantBuffer().sun_intensity_;

	float3 base_color = instance.texture_.base_color_.rgb;
	if (instance.texture_.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.texture_.base_color_texture_index_];
		base_color *= base_color_texture.Sample(sampler_linear_wrap, input.texcoord).rgb;
	}

	float fur_shininess = lerp(48.0, 4.0, instance.texture_.roughness_);
	float3 f0 = float3(0.04, 0.04, 0.04);
	float3 direct = EvalDirectLightFur(normal, fibre_tangent, view, light_direction, base_color, f0, fur_shininess) * light_color;

	float3 sky_color = float3(0.55, 0.60, 0.68);
	float3 ground_color = float3(0.20, 0.18, 0.17);
	float3 ambient = lerp(ground_color, sky_color, normal.y * 0.5 + 0.5) * base_color;

	// darken towards the skin (self-shadowing), lighten the tips.
	float ao = lerp(0.35, 1.0, input.shell_ratio);
	float3 lit = (direct + ambient * 0.5) * ao;

	// thin out the outer shells so the coat has a soft silhouette.
	float alpha = saturate((strand_height - input.shell_ratio) * 6.0) * (1.0 - input.shell_ratio * 0.35);

	return float4(lit, alpha);
}
