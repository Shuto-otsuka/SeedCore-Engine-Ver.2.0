#include "../Shader/Constants.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Normal.hlsli"
#include "../Model/Model.hlsli"

/**
* [EN]
* Synthesizes the RGB=normal / A=roughness texture NVIDIA DLSS Ray
* Reconstruction expects (`DlssBufferTag::normalRoughnessBuffer_`) from the
* G-Buffer's packed RT1 (`gbuffer1.rg` = octahedral-encoded world-space
* normal, `gbuffer1.b` = roughness — same decode as
* Model/Opaque/DeferredLightingPS.hlsl). DLSS-RR wants a plain float normal+
* roughness channel, not the engine's octahedral packing, so this is a tiny
* full-screen decode pass run once per view right before DLSS-RR's Tag()/
* Evaluate() for that view.
*
* [JP]
* NVIDIA DLSS Ray Reconstruction が要求する RGB=法線 / A=ラフネスのテクスチャ
* (`DlssBufferTag::normalRoughnessBuffer_`)を、G-Buffer のパック済み RT1
* (`gbuffer1.rg`=八面体エンコードされたワールド空間法線、`gbuffer1.b`=
* ラフネス — Model/Opaque/DeferredLightingPS.hlsl と同じデコード)から合成する。
* DLSS-RR はエンジン独自の八面体パッキングではなく素の float 法線+ラフネス
* チャンネルを要求するため、各ビューの DLSS-RR Tag()/Evaluate() の直前に
* 1回だけ走る小さな全画面デコードパス。
*/
float3 EnvBRDFApprox2(float3 specular_color, float alpha, float n_o_v)
{
	n_o_v = abs(n_o_v);
	float4 x;
	x.x = 1.0;
	x.y = n_o_v;
	x.z = n_o_v * n_o_v;
	x.w = n_o_v * x.z;
	float4 y;
	y.x = 1.0;
	y.y = alpha;
	y.z = alpha * alpha;
	y.w = alpha * y.z;
	float2x2 m1 = float2x2(0.99044f, -1.28514f, 1.29678f, -0.755907f);
	float3x3 m2 = float3x3(1.0f, 2.92338f, 59.4188f, 20.3225f, -27.0302f, 222.592f, 121.563f, 626.13f, 316.627f);
	float2x2 m3 = float2x2(0.0365463f, 3.32707f, 9.0632f, -9.04756f);
	float3x3 m4 = float3x3(1.0f, 3.59685f, -1.36772f, 9.04401f, -16.3174f, 9.22949f, 5.56589f, 19.7886f, -20.2123f);
	float bias = dot(mul(m1, x.xy), y.xy) * rcp(dot(mul(m2, x.xyw), y.xyw));
	float scale = dot(mul(m3, x.xy), y.xy) * rcp(dot(mul(m4, x.xzw), y.xyw));
	bias *= saturate(specular_color.g * 50.0);
	return mad(specular_color, max(0.0, scale), max(0.0, bias));
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	uint2 pixel = dtid.xy;

	Texture2D<float4> gbuffer0 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_0_];
	Texture2D<float4> gbuffer1 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	Texture2D<uint2> gbuffer4 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_4_];
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	RWTexture2D<float4> normal_roughness = ResourceDescriptorHeap[constant_indices.dlss_.normal_roughness_uav_index_];
	RWTexture2D<float4> specular_albedo = ResourceDescriptorHeap[constant_indices.dlss_.specular_albedo_uav_index_];
	RWTexture2D<float4> diffuse_albedo = ResourceDescriptorHeap[constant_indices.dlss_.diffuse_albedo_uav_index_];

	float4 packed = gbuffer1.Load(int3(pixel, 0));
	float3 normal = OctNormalDecode(packed.rg);
	float roughness = packed.b;

	normal_roughness[pixel] = float4(normal, roughness);

	float depth = depth_texture.Load(int3(pixel, 0));
	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 view_direction = normalize(scene.camera_position_.xyz - world.xyz / world.w);
	float n_o_v = dot(normal, view_direction);

	float4 base_color_metallic = gbuffer0.Load(int3(pixel, 0));

	/// [JP] KHR_materials_ior/specular はGBufferに焼き込まれていないので、
	///      VisID(RT4)からinstance_indexを引いてModelInstanceからその場で
	///      誘電体F0を計算する(Model/Opaque/DeferredLightingPS.hlsl と同じ式)。
	uint2 visibility_id = gbuffer4.Load(int3(pixel, 0));
	uint material_instance_index, material_meshlet_index, material_triangle_index;
	UnpackVisibilityID(visibility_id, material_instance_index, material_meshlet_index, material_triangle_index);
	StructuredBuffer<ModelInstance> material_instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance material_instance = material_instances[material_instance_index];
	float dielectric = (material_instance.ior_ - 1.0) / (material_instance.ior_ + 1.0);
	dielectric *= dielectric;
	float3 dielectric_f0 = saturate(dielectric * material_instance.specular_color_ * material_instance.specular_factor_);
	float3 specular_color = lerp(dielectric_f0, base_color_metallic.rgb, base_color_metallic.a);
	specular_albedo[pixel] = float4(EnvBRDFApprox2(specular_color, roughness * roughness, n_o_v), 1.0);

	diffuse_albedo[pixel] = float4(base_color_metallic.rgb * (1.0 - base_color_metallic.a), 1.0);
}
