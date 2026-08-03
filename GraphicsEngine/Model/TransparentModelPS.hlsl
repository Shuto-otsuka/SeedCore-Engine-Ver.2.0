#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"
#include "../Shader/Constants.hlsli"

/**
* [EN]
* Pixel Shader for transparent model PPLL accumulation.
* Instead of writing to G-Buffer render targets, this PS allocates
* a fragment in the Per-Pixel Linked List and writes color + depth.
* Used by both static and skeletal transparent models.
*
* The head pointer texture, fragment buffer, and counter are accessed
* as UAVs via the bindless descriptor heap.
*
* ---------------------------------------------------------------------
*
* [JP]
* 透明モデル PPLL 蓄積用のピクセルシェーダー。
* G-Buffer RT に書き込む代わりに、Per-Pixel Linked List にフラグメントを
* アトミックに割り当て、色 + 深度を書き込む。
* 静的・スケルタル両方の透明モデルで使用する。
*
* ヘッドポインタテクスチャ、フラグメントバッファ、カウンターは
* バインドレスディスクリプタヒープ経由で UAV としてアクセスする。
*/
void main(ModelMSOutput input)
{
	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance instance = instances[input.instance_index];

	float4 base_color = instance.base_color_;
	if (instance.base_color_texture_index_ != OIT_INVALID_INDEX)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.base_color_texture_index_];
		base_color *= base_color_texture.Sample(sampler_aniso_wrap, input.texcoord);
	}

	clip(base_color.a - 0.01);

	float3 emissive = instance.emissive_;
	if (instance.emissive_texture_index_ != OIT_INVALID_INDEX)
	{
		Texture2D emissive_texture = ResourceDescriptorHeap[instance.emissive_texture_index_];
		emissive *= emissive_texture.Sample(sampler_linear_wrap, input.texcoord).rgb;
	}

	float4 final_color = float4(base_color.rgb + emissive, base_color.a);

	RWTexture2D<uint> head_pointer = ResourceDescriptorHeap[structured_indices.oit_.head_pointer_index_];
	RWStructuredBuffer<OITFragment> fragment_buffer = ResourceDescriptorHeap[structured_indices.oit_.fragment_buffer_index_];
	RWByteAddressBuffer oit_counter = ResourceDescriptorHeap[structured_indices.oit_.counter_index_];

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	uint max_fragments = uint(scene.screen_size_.x) * uint(scene.screen_size_.y) * OIT_MAX_LAYERS;

	uint new_index;
	oit_counter.InterlockedAdd(0, 1, new_index);
	if (new_index >= max_fragments)
	{
		return;
	}

	uint2 pixel = uint2(input.position.xy);
	uint old_head;
	InterlockedExchange(head_pointer[pixel], new_index, old_head);

	OITFragment frag;
	frag.packed_color_ = PackColorRGBA8(final_color);
	frag.depth_ = input.position.z;
	frag.next_ = old_head;
	fragment_buffer[new_index] = frag;
}
