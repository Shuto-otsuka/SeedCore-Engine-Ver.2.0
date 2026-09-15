#ifndef __RAYTRACING_HLSL__
#define __RAYTRACING_HLSL__

struct RaytracingShaderResourceIndices
{
	uint tlas_index_;
	uint instance_data_index_;
	uint2 raytracing_shader_resource_padding_0_;
};

#endif // __RAYTRACING_HLSL__
