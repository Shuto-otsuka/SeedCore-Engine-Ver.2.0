#ifndef __SCENE_HLSL__
#define __SCENE_HLSL__

#include "Constants.hlsli"

struct SceneConstantBuffer
{
	row_major float4x4 view_;
	row_major float4x4 inverse_view_;

	row_major float4x4 projection_;
	row_major float4x4 inverse_projection_;
	row_major float4x4 non_jitter_projection_;

	row_major float4x4 current_view_projection_;
	row_major float4x4 previous_view_projection_;
	row_major float4x4 inverse_view_projection_;
	row_major float4x4 non_jitter_view_projection_;
	row_major float4x4 previous_non_jitter_view_projection_;

	float4 camera_position_;
	float4 camera_focus_;

	float field_of_view_;
	float near_plane_;
	float far_plane_;
	uint view_mode_;

	float total_time_;
	float delta_time_;

	// Resolution of whatever the post-tonemap debug overlay (collider wireframes, selection outline) actually draws onto - screen_size_ normally, or PostProcessRenderer's DLSS-RR-upscaled output resolution while DLSS-RR is active. SelectionOutlinePS.hlsl scales SV_Position by screen_size_/display_size_ before indexing the (native-resolution) selection mask.
	float2 display_size_;

	float2 screen_size_;
	float2 inverse_screen_size_;
};

ConstantBuffer<SceneConstantBuffer> GetSceneConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.scene_index_];
}

#endif // __SCENE_HLSL__
