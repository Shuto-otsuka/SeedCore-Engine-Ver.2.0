#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"
#include "../Shader/Normal.hlsli"

/**
* [EN]
* Pixel Shader for skeletal model G-Buffer output.
* Identical to StaticModelPS — skinning is fully handled in the MS,
* so the PS receives the same interpolated data.
*
* ---------------------------------------------------------------------
*
* [JP]
* スケルタルモデル G-Buffer 出力用のピクセルシェーダー。
* StaticModelPS と同一 — スキニングは MS で完全に処理されるため、
* PS は同じ補間データを受け取る。
*/
ModelPSOutput main(ModelMSOutput input, bool is_front_face : SV_IsFrontFace)
{
	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance instance = instances[input.instance_index];

	float4 base_color = instance.base_color_;
	if (instance.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.base_color_texture_index_];
		base_color *= base_color_texture.Sample(sampler_aniso_wrap, input.texcoord);
	}

	clip(base_color.a - instance.alpha_cutoff_);

	float3 N = normalize(input.world_normal);

	/// [JP] 両面描画(CULL_NONE)の裏面は法線が視線と逆を向くので反転する。
	///      TBN 構築より前に行い、法線マップも反転後の N で正しく乗るようにする。
	if (!is_front_face)
	{
		N = -N;
	}

	if (instance.normal_texture_index_ != 0xFFFFFFFF)
	{
		float3 T = normalize(input.world_tangent.xyz);
		float3 B = cross(N, T) * input.world_tangent.w;
		float3x3 TBN = float3x3(T, B, N);

		Texture2D normal_texture = ResourceDescriptorHeap[instance.normal_texture_index_];
		float3 normal_sample = normal_texture.Sample(sampler_linear_wrap, input.texcoord).xyz * 2.0 - 1.0;
		N = normalize(mul(normal_sample, TBN));
	}

	float metallic = instance.metallic_;
	float roughness = instance.roughness_;
	if (instance.metallic_roughness_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D metallic_roughness_texture = ResourceDescriptorHeap[instance.metallic_roughness_texture_index_];
		float4 metallic_roughness = metallic_roughness_texture.Sample(sampler_linear_wrap, input.texcoord);
		metallic *= metallic_roughness.b;
		roughness *= metallic_roughness.g;
	}

	float3 emissive = instance.emissive_;
	if (instance.emissive_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D emissive_texture = ResourceDescriptorHeap[instance.emissive_texture_index_];
		emissive *= emissive_texture.Sample(sampler_linear_wrap, input.texcoord).rgb;
	}

	float2 current_ndc = input.current_position.xy / input.current_position.w;
	float2 previous_ndc = input.previous_position.xy / input.previous_position.w;
	float2 velocity = (current_ndc - previous_ndc) * 0.5;

	/// [JP] KHR_materials_ior / specular を誘電体 F0 に畳んで RT2.gba へ。
	float dielectric = (instance.ior_ - 1.0) / (instance.ior_ + 1.0);
	dielectric *= dielectric;
	float3 dielectric_f0 = saturate(dielectric * instance.specular_color_ * instance.specular_factor_);

	ModelPSOutput output;
	output.base_color_metallic = float4(base_color.rgb, metallic);
	/// [JP] RT1.a = pack8x8(clearcoat_factor, clearcoat_roughness)。
	output.normal_roughness = float4(OctNormalEncode(N), roughness, PackUnorm8x8(instance.clearcoat_factor_, instance.clearcoat_roughness_));
	/// [JP] RT2 = (anisotropy, dielectric_f0.rgb)。
	output.material_extension = float4(instance.anisotropy_, dielectric_f0);
	output.velocity = velocity;
	/// [JP] emissive_strength を焼き込む。
	output.emissive = float4(emissive * instance.emissive_strength_, 0.0);

	return output;
}
