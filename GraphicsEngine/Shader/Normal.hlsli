#ifndef __NORMAL_HLSL__
#define __NORMAL_HLSL__

float2 OctNormalEncode(float3 n)
{
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	if (n.z < 0.0)
	{
		n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
	}
	return n.xy * 0.5 + 0.5;
}

float3 OctNormalDecode(float2 e)
{
	e = e * 2.0 - 1.0;
	float3 n = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	if (n.z < 0.0)
	{
		n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
	}
	return normalize(n);
}

// Orthonormal basis around a unit normal (Duff et al. branchless variant).
// Deliberately a private copy rather than Noise.hlsli's BuildOrthonormalBasis:
// this header is self-contained, and the tangent pack/unpack below only needs
// the writer and the reader to agree with EACH OTHER, not with the sampling
// code. Renamed so a shader including both headers still compiles.
void NormalOrthonormalBasis(float3 normal, out float3 tangent, out float3 bitangent)
{
	float s = normal.z >= 0.0 ? 1.0 : -1.0;
	float a = -1.0 / (s + normal.z);
	float b = normal.x * normal.y * a;
	tangent   = float3(1.0 + s * normal.x * normal.x * a, s * b, -s * normal.x);
	bitangent = float3(b, s + normal.y * normal.y * a, -normal.y);
}

// Packs a tangent into ONE 16-bit UNORM channel (G-Buffer RT1.a). A tangent is
// perpendicular to its normal, so it is fully described by a single rotation
// angle within a reference frame built from that normal - no second channel
// needed. The angle takes 15 bits (~0.011 degrees, far finer than a normal map
// needs) and the handedness sign takes the top bit.
float PackTangentAngle(float3 normal, float3 tangent, float handedness)
{
	float3 basis_tangent, basis_bitangent;
	NormalOrthonormalBasis(normal, basis_tangent, basis_bitangent);

	float angle = atan2(dot(tangent, basis_bitangent), dot(tangent, basis_tangent));
	float angle01 = saturate(angle * (0.5 / 3.14159265358979) + 0.5);

	float quantized = floor(angle01 * 32767.0 + 0.5);
	return (quantized + (handedness < 0.0 ? 32768.0 : 0.0)) / 65535.0;
}

void UnpackTangentAngle(float3 normal, float packed, out float3 tangent, out float handedness)
{
	float scaled = floor(packed * 65535.0 + 0.5);
	handedness = scaled >= 32768.0 ? -1.0 : 1.0;

	float quantized = scaled >= 32768.0 ? scaled - 32768.0 : scaled;
	float angle = (quantized / 32767.0 - 0.5) * (2.0 * 3.14159265358979);

	float3 basis_tangent, basis_bitangent;
	NormalOrthonormalBasis(normal, basis_tangent, basis_bitangent);
	tangent = normalize(cos(angle) * basis_tangent + sin(angle) * basis_bitangent);
}

#endif // __NORMAL_HLSL__
