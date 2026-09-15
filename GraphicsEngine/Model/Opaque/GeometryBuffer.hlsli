#ifndef __GEOMETRY_BUFFER_HLSL__
#define __GEOMETRY_BUFFER_HLSL__

struct GeometryBufferShaderResourceIndices
{
	uint index_0_;                    // RT0: base_color.rgb + metallic
	uint index_1_;                    // RT1: octNormal.rg + roughness (.a unused)
	uint index_2_;                    // RT2: velocity
	uint index_3_;                    // RT3: emissive.rgb (raw, emissive_strength_ applied at lighting time)

	uint index_4_;                    // RT4: VisibilityBuffer id (instance/meshlet/triangle) + asuint(texcoord), R32G32B32A32_UINT
	uint depth_index_;
	uint2 geometry_buffer_shader_resource_padding_0_;
};

struct GeometryBufferUnorderedAccessIndices
{
	uint index_0_;
	uint index_1_;
	uint index_2_;
	uint index_3_;
};

#endif // __GEOMETRY_BUFFER_HLSL__
