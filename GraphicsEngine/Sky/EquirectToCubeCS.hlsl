#include "Sky.hlsli"
#include "SkyGenerate.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Projects an equirectangular HDR (Texture2D) onto the six faces of the
* environment cube (mip 0). One thread per destination cube texel.
*
* [JP]
* パノラマ HDR（Texture2D）を environment キューブの 6 面（ミップ 0）へ投影
* する。出力キューブテクセル 1 つにつき 1 スレッド。
*/
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint size = GetSkyDispatchBuffer().face_size_;
	if (id.x >= size || id.y >= size)
	{
		return;
	}

	uint face = id.z + GetSkyDispatchBuffer().face_offset_;
	float2 uv = (float2(id.xy) + 0.5) / float(size) * 2.0 - 1.0;
	float3 dir = CubeFaceDirection(face, uv);

	Texture2D<float4> source = ResourceDescriptorHeap[GetSkyDispatchBuffer().source_index_];
	RWTexture2DArray<float4> destination = ResourceDescriptorHeap[GetSkyDispatchBuffer().dest_index_];

	float2 equirect_uv = EquirectangularUv(dir);
	float4 color = source.SampleLevel(sampler_linear_clamp, equirect_uv, 0);

	destination[uint3(id.xy, face)] = color;
}
