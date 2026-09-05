#ifndef __FROXEL_HLSL__
#define __FROXEL_HLSL__

/**
* Shared helpers for the froxel (frustum-voxel) volumetric pipeline.
* Used by: Fog injection (pass 1), VolumetricLight scattering (pass 2, RT),
*          Froxel integration (pass 3), and the deferred composite (sampling).
*
* Resource convention:
*   density volume    RWTexture3D<float4> : rgb = scattering coefficient, a = extinction
*   scattering volume RWTexture3D<float4> : rgb = in-scattered light,      a = extinction
*   integrated volume RWTexture3D<float4> : rgb = accumulated scattering,  a = transmittance
*/

/**
* Exponential depth slicing: the near side is denser (more froxels close to
* the camera, where volumetric detail matters most). slice_t in [0,1] ->
* linear view-space Z.
*/
float FroxelSliceToViewZ(float slice_t, float near_plane, float far_plane)
{
	return near_plane * pow(far_plane / near_plane, slice_t);
}

/**
* Inverse of FroxelSliceToViewZ: view-space Z -> froxel Z slice_t in [0,1].
*/
float ViewZToFroxelSlice(float view_z, float near_plane, float far_plane)
{
	return log(view_z / near_plane) / log(far_plane / near_plane);
}

/**
* Froxel integer coordinate -> world position. froxel_dimensions is e.g.
* (160, 90, 64). Uses the exponential slice mapping above for Z.
*/
float3 FroxelToWorld(uint3 froxel, uint3 froxel_dimensions, float near_plane, float far_plane, float4x4 inverse_view_projection)
{
	float2 uv = (float2(froxel.xy) + 0.5) / float2(froxel_dimensions.xy);
	float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

	float slice_t = (float(froxel.z) + 0.5) / float(froxel_dimensions.z);
	float view_z = FroxelSliceToViewZ(slice_t, near_plane, far_plane);

	/// Project the linear view Z back to a reverse-Z device depth, then
	/// unproject. reverse-Z: device_depth = near / view_z (for an
	/// infinite-far reverse-Z projection).
	float device_depth = near_plane / view_z;
	float4 clip = float4(ndc, device_depth, 1.0);
	float4 world = mul(clip, inverse_view_projection);
	return world.xyz / world.w;
}

/**
* World position -> froxel UVW in [0,1]^3 (for sampling the integrated
* volume). Only the depth-slice Z component is meaningful here - NDC xy from
* the view position needs the projection matrix, which callers typically
* already have their own screen-space uv for, so only .z is filled in and
* callers supply xy themselves. Kept as a helper for the depth slice only.
*/
float3 WorldToFroxelUVW(float3 world_position, float4x4 view, float near_plane, float far_plane)
{
	float4 view_position = mul(float4(world_position, 1.0), view);
	float view_z = view_position.z;

	float slice_t = saturate(ViewZToFroxelSlice(view_z, near_plane, far_plane));
	return float3(0.0, 0.0, slice_t);
}

/**
* Henyey-Greenstein phase function. cos_theta = dot(view_dir, light_dir). g
* controls the scattering lobe: 0 = isotropic, positive = forward-scattering
* (toward the light, the classic "god ray" look), negative = back-scattering.
*/
float PhaseHenyeyGreenstein(float cos_theta, float g)
{
	const float PI = 3.14159265358979;
	float g2 = g * g;
	float denom = 1.0 + g2 - 2.0 * g * cos_theta;
	return (1.0 - g2) / (4.0 * PI * pow(max(denom, 1e-4), 1.5));
}

#endif // __FROXEL_HLSL__
