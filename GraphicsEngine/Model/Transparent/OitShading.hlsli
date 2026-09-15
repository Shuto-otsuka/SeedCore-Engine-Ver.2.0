#ifndef __OIT_SHADING_HLSL__
#define __OIT_SHADING_HLSL__

#include "../../Shader/Constants.hlsli"

struct OitConstantBuffer
{
	// OITFragment elements actually allocated. Supplied by OITBuffer, not
	// recomputed from screen size - the pool is clamped to a byte budget.
	uint fragment_capacity_;
	uint3 oit_constant_padding_0_;
};

ConstantBuffer<OitConstantBuffer> GetOitConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.oit_index_];
}

#endif // __OIT_SHADING_HLSL__
