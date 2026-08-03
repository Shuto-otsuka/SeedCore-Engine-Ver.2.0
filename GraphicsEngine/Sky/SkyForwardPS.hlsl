#include "../Model/Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"
#include "../Shader/Light.hlsli"
#include "../Shader/Sampler.hlsli"
#include "../Light/ImageBasedLighting.hlsli"

/**
* [EN]
* Forward pixel shader for the Realtime skymap capture. Renders opaque model
* meshlets into one environment-cube face with cheap lighting (base color x
* directional N.L + flat ambient + emissive). Deliberately simple: the result
* is convolved into the irradiance / prefiltered cubes, so high fidelity would
* be wasted, and it must not read the IBL cubes it feeds (feedback loop).
* Output is the cube face format (R16G16B16A16_FLOAT), single render target.
*
* ---------------------------------------------------------------------
*
* [JP]
* Realtime スカイマップキャプチャ用フォワードピクセルシェーダー。不透明モデル
* のメシュレットを environment キューブ 1 面へ、簡易ライティング（baseColor ×
* directional N.L + フラット環境光 + emissive）で描く。あえて簡素: 結果は
* irradiance / prefilter キューブへ畳み込まれるので高精細は無駄で、かつ自分が
* 供給する IBL キューブを読んではいけない（フィードバックループ）。出力は
* キューブ面フォーマット（R16G16B16A16_FLOAT）の単一レンダーターゲット。
*/
static const float SKY_FORWARD_AMBIENT = 0.1;

float4 main(ModelMSOutput input) : SV_Target0
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

	float3 normal = normalize(input.world_normal);
	float3 diffuse = base_color.rgb;

	/// [EN] Ambient from the sky-only environment cube (no feedback: it never
	///      contains the captured scene), so surfaces the directional light
	///      misses are lit by the actual sky colour instead of going near-black.
	/// [JP] 環境光は空だけの environment キューブから取る（フィードバック無し:
	///      キャプチャしたシーンは含まれない）。DirectionalLight が当たらない面も
	///      真っ黒にならず、実際の空色で照らされる。
	float3 ambient = float3(SKY_FORWARD_AMBIENT, SKY_FORWARD_AMBIENT, SKY_FORWARD_AMBIENT);
	if (structured_indices.sky_.environment_cube_index_ != 0)
	{
		ambient = SampleSkyboxEnvironment(normal).rgb;
	}

	float3 lighting = diffuse * ambient;

	ConstantBuffer<LightConstantData> light_constant_buffer = ResourceDescriptorHeap[constant_indices.light_index_];
	if (light_constant_buffer.directional_intensity_ > 0.0)
	{
		float3 light_direction = normalize(-light_constant_buffer.directional_direction_);
		float normal_dot_light = max(dot(normal, light_direction), 0.0);
		lighting += diffuse * normal_dot_light * light_constant_buffer.directional_color_.rgb * light_constant_buffer.directional_intensity_;
	}

	return float4(lighting + instance.emissive_, 1.0);
}
