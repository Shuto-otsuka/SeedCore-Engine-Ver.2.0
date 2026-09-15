#ifndef __SKY_GENERATE_HLSL__
#define __SKY_GENERATE_HLSL__

#include "../Shader/Dispatch.hlsli"
#include "../Shader/Constants.hlsli"

// Sky's own contribution to image-based lighting, read by every shader that
// samples the environment cube/irradiance/prefiltered maps (shader_resource_indices'
// SkyShaderResourceIndices) for diffuse/specular IBL.
struct SkyConstantBuffer
{
	float intensity_;
	float3 sky_constant_padding_0_;
};

ConstantBuffer<SkyConstantBuffer> GetSkyConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.sky_index_];
}

// Per-dispatch constants for the skymap IBL generation compute passes, reached
// through the shared per-dispatch root constant (params[3], see Dispatch.hlsli)
// rather than a dedicated root CBV - this slot is reused across every system
// with its own per-dispatch private data (Zephyr's ParticleDispatchBuffer,
// this), never bound at the same time as any of the others. All texture
// handles are bindless indices into ResourceDescriptorHeap. ASCII only
// (included file).

struct SkyDispatchBuffer
{
	uint source_index_;   // SRV: equirect Texture2D or environment cube.
	uint dest_index_;     // UAV: destination cube mip (Texture2DArray) or LUT.
	uint face_size_;      // Destination face resolution at this mip.
	uint sample_count_;   // Convolution / integration sample count.
	float roughness_;     // Prefilter roughness for this mip.
	uint mip_level_;      // Destination mip level.
	uint face_offset_;    // Added to id.z: lets a dispatch target one cube face.
	uint sky_dispatch_buffer_padding_0_;
};

ConstantBuffer<SkyDispatchBuffer> GetSkyDispatchBuffer()
{
	return ResourceDescriptorHeap[dispatch_buffer_index_];
}

#endif // __SKY_GENERATE_HLSL__
