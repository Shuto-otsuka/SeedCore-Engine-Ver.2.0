#ifndef __SKY_GENERATE_HLSL__
#define __SKY_GENERATE_HLSL__

// Per-dispatch constants for the skymap IBL generation compute passes. Bound to
// root parameter 2 (CBV b0, space1). All texture handles are bindless indices
// into ResourceDescriptorHeap. ASCII only (included file).

struct SkyGenerateConstant
{
	uint source_index_;   // SRV: equirect Texture2D or environment cube.
	uint dest_index_;     // UAV: destination cube mip (Texture2DArray) or LUT.
	uint face_size_;      // Destination face resolution at this mip.
	uint sample_count_;   // Convolution / integration sample count.
	float roughness_;     // Prefilter roughness for this mip.
	uint mip_level_;      // Destination mip level.
	uint face_offset_;    // Added to id.z: lets a dispatch target one cube face.
	uint sky_generate_padding_;
};
ConstantBuffer<SkyGenerateConstant> sky_generate : register(b0, space1);

#endif // __SKY_GENERATE_HLSL__
