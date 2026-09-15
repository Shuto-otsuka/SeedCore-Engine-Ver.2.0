#ifndef __DLSS_HLSL__
#define __DLSS_HLSL__

/**
* [EN]
* DLSS Ray Reconstruction's synthesized RGB=normal/A=roughness buffer for
* this view. Genuinely per-view (Editor's and Game's G-Buffer content
* differ) - set once at DlssRayReconstructionRenderer::Create() time, not
* per-frame (single-buffered per view, never ping-ponged).
*/
struct DlssUnorderedAccessIndices
{
	uint normal_roughness_index_;
	uint specular_albedo_index_;
	uint diffuse_albedo_index_;
	uint dlss_unordered_access_padding_0_;
};

#endif // __DLSS_HLSL__
