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

// GGX importance-sampled half vector around a normal.
float3 ImportanceSampleGgx(float2 xi, float3 normal, float roughness)
{
	float alpha = roughness * roughness;

	float phi = 2.0 * SKY_PI * xi.x;
	float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
	float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

	float3 half_tangent = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

	float3 up = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent = normalize(cross(up, normal));
	float3 bitangent = cross(normal, tangent);

	return normalize(tangent * half_tangent.x + bitangent * half_tangent.y + normal * half_tangent.z);
}

#endif // __SKY_MATH_HLSL__
