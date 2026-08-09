#include "../Shader/Constants.hlsli"

/**
* [EN]
* The final display pass: reads ToneMappingCS.hlsl's tone-mapped, sRGB-
* encoded UNORM output (now an intermediate buffer, not the final display
* texture) and applies a 5-tap cross unsharp mask, pushing each pixel away
* from its 4-neighbor average by amount_ - the classic sharpen/edge-
* enhancement look. Runs unconditionally every frame regardless of
* SharpnessSettings.enabled_, same reasoning as ToneMappingCS.hlsl's sRGB
* encode: this pass owns the buffer PostProcessRenderer::OutputResource et
* al. now point at, so it must always produce a complete image - enabled_
* just multiplies the sharpen offset by 0, making the shader a pure copy
* when off.
*
* ---------------------------------------------------------------------
*
* [JP]
* 最終表示パス: ToneMappingCS.hlsl のトーンマップ済み・sRGBエンコード済み
* UNORM出力(今は中間バッファ、最終表示テクスチャではない)を読み、5タップの
* 十字アンシャープマスクを適用し、各ピクセルを4近傍平均から amount_ ぶん
* 引き離す — 典型的なシャープ/輪郭強調の見た目。SharpnessSettings.enabled_
* に関わらず毎フレーム無条件で走る、ToneMappingCS.hlsl の sRGB エンコードと
* 同じ理由: このパスは PostProcessRenderer::OutputResource 等が指す
* バッファを持つので、常に完全な画像を作らなければならない — enabled_ は
* シャープオフセットに0を掛けるだけで、オフの時はシェーダが単純コピーに
* なる。
*/
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[constant_indices.post_process_.sharpness_.destination_uav_index_];

	uint width, height;
	destination.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.sharpness_.source_srv_index_];

	int2 pixel = int2(dtid.xy);
	int2 pixel_max = int2(width - 1, height - 1);

	float3 center = source.Load(int3(pixel, 0)).rgb;
	float3 north = source.Load(int3(clamp(pixel + int2(0, -1), int2(0, 0), pixel_max), 0)).rgb;
	float3 south = source.Load(int3(clamp(pixel + int2(0, 1), int2(0, 0), pixel_max), 0)).rgb;
	float3 west = source.Load(int3(clamp(pixel + int2(-1, 0), int2(0, 0), pixel_max), 0)).rgb;
	float3 east = source.Load(int3(clamp(pixel + int2(1, 0), int2(0, 0), pixel_max), 0)).rgb;

	float3 blur = (north + south + west + east) * 0.25;
	float amount = constant_indices.post_process_.sharpness_.amount_ * (constant_indices.post_process_.sharpness_.enabled_ != 0 ? 1.0 : 0.0);
	float3 color = saturate(center + (center - blur) * amount);

	destination[dtid.xy] = float4(color, 1.0);
}
