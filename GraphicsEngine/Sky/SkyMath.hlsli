#ifndef __SKY_MATH_HLSL__
#define __SKY_MATH_HLSL__

// Math helpers shared by the skymap IBL generation compute passes:
// cube-face directions, equirectangular mapping and GGX importance sampling.
// ASCII only (this file is included; DXC breaks on non-ASCII in .hlsli).

static const float SKY_PI = 3.14159265358979;
static const float SKY_INV_2PI = 0.15915494309189; // 1 / (2*pi)
static const float SKY_INV_PI = 0.31830988618379;  // 1 / pi

// World-space direction for a cube face texel. face follows the D3D12 cube
// order (+X, -X, +Y, -Y, +Z, -Z); uv is in [-1, 1] across the face.
float3 CubeFaceDirection(uint face, float2 uv)
{
	float3 dir;
	switch (face)
	{
	case 0: dir = float3(1.0, -uv.y, -uv.x); break; // +X
	case 1: dir = float3(-1.0, -uv.y, uv.x); break; // -X
	case 2: dir = float3(uv.x, 1.0, uv.y); break;   // +Y
	case 3: dir = float3(uv.x, -1.0, -uv.y); break; // -Y
	case 4: dir = float3(uv.x, -uv.y, 1.0); break;  // +Z
	default: dir = float3(-uv.x, -uv.y, -1.0); break; // -Z
	}
	return normalize(dir);
}

// Equirectangular uv for a direction (matches DirectXTex HDR layout).
float2 EquirectangularUv(float3 dir)
{
	float2 uv = float2(atan2(dir.z, dir.x), asin(clamp(dir.y, -1.0, 1.0)));
	uv *= float2(SKY_INV_2PI, SKY_INV_PI);
	uv += 0.5;
	uv.y = 1.0 - uv.y;
	return uv;
}

// Van der Corput radical inverse (base 2) for low-discrepancy sampling.
float RadicalInverseVanDerCorput(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint count)
{
	return float2(float(i) / float(count), RadicalInverseVanDerCorput(i));
}

// GGX visible-normal (VNDF) sampled half vector, spherical-cap construction.
// view points away from the surface (surface -> camera).
float3 SampleGgxVisibleNormal(float2 xi, float3 normal, float3 view, float roughness)
{
	float alpha = roughness * roughness;

	float sign_z = normal.z >= 0.0 ? 1.0 : -1.0;
	float a = 1.0 / (sign_z + normal.z);
	float ya = normal.y * a;
	float b = normal.x * ya;
	float c = normal.x * sign_z;

	float3 tangent = float3(c * normal.x * a - 1.0, sign_z * b, c);
	float3 bitangent = float3(b, normal.y * ya - sign_z, normal.y);

	float3 view_tangent = float3(dot(view, tangent), dot(view, bitangent), dot(view, normal));

	float3 view_hemisphere = normalize(float3(view_tangent.xy * alpha, view_tangent.z));

	float phi = 2.0 * SKY_PI * xi.x;
	float z = (1.0 - xi.y) * (1.0 + view_hemisphere.z) - view_hemisphere.z;
	float sin_theta = sqrt(saturate(1.0 - z * z));
	float3 cap = float3(sin_theta * cos(phi), sin_theta * sin(phi), z);

	float3 half_hemisphere = cap + view_hemisphere;
	float3 half_tangent = normalize(float3(half_hemisphere.xy * alpha, half_hemisphere.z));

	return normalize(tangent * half_tangent.x + bitangent * half_tangent.y + normal * half_tangent.z);
}

// Height-correlated Smith G2/G1 - the entire weight a VNDF-sampled Smith-GGX
// sample carries besides Fresnel. 1 for a mirror, 0 below the horizon.
float SmithGgxG2OverG1(float normal_dot_view, float normal_dot_light, float roughness)
{
	if (normal_dot_light <= 0.0)
	{
		return 0.0;
	}

	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;

	float lambda_view = sqrt(alpha2 + (1.0 - alpha2) * normal_dot_view * normal_dot_view);
	float lambda_light = sqrt(alpha2 + (1.0 - alpha2) * normal_dot_light * normal_dot_light);

	return normal_dot_light * (normal_dot_view + lambda_view) / max(normal_dot_view * lambda_light + normal_dot_light * lambda_view, 0.0001);
}

// GGX importance-sampled half vector around a normal.
float3 ImportanceSampleGgx(float2 xi, float3 normal, float roughness)
{
	float alpha = roughness * roughness;

	float phi = 2.0 * SKY_PI * xi.x;
	float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
	float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

	float3 half_tangent = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

	// Branchless Frisvad/Duff orthonormal basis. The up-vector switch this replaces
	// picks between two axes FIXED in world space depending on normal.z, so
	// any surface whose normal falls near either fixed axis - not just at the
	// exact threshold - gets a tangent frame that is nearly degenerate
	// (cross(up, normal) close to zero before normalize renormalizes the
	// error back up), injecting extra sampling noise there. For a sphere,
	// that shows up as a region of visibly noisier reflection wherever the
	// surface normal happens to pass near that fixed axis - not spread evenly
	// over the surface, and not fixed to any particular part of the sphere as
	// seen on screen, since it depends on the normal's direction in WORLD
	// space rather than anything screen-relative. This construction has no
	// such axis: it is continuous and well-conditioned for every normal
	// direction.
	float sign_z = normal.z >= 0.0 ? 1.0 : -1.0;
	float a = 1.0 / (sign_z + normal.z);
	float ya = normal.y * a;
	float b = normal.x * ya;
	float c = normal.x * sign_z;

	float3 tangent = float3(c * normal.x * a - 1.0, sign_z * b, c);
	float3 bitangent = float3(b, normal.y * ya - sign_z, normal.y);

	return normalize(tangent * half_tangent.x + bitangent * half_tangent.y + normal * half_tangent.z);
}

#endif // __SKY_MATH_HLSL__
