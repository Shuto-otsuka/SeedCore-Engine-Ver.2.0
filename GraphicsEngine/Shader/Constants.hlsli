#ifndef __CONSTANTS_HLSL__
#define __CONSTANTS_HLSL__

struct ConstantIndices
{
	uint scene_index_;
	uint light_index_;
	uint cluster_assign_index_;
	uint post_process_index_;

	uint weather_index_;
	uint directional_light_index_;
	uint sky_index_;
	uint oit_index_;

	uint fur_index_;
	uint shadow_index_;
	uint ambient_occlusion_index_;
	uint subsurface_scattering_index_;

	uint reflection_index_;
	uint refraction_index_;
	uint global_illumination_index_;
	uint cloud_index_;

	uint star_index_;
	uint weather_particle_index_;
	uint volumetric_light_index_;
	uint collider_index_;
};
ConstantBuffer<ConstantIndices> constant_indices : register(b2, space1);

#endif // __CONSTANTS_HLSL__
