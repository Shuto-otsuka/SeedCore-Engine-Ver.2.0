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

// Pack two [0,1] values into a single 16-bit UNORM channel (8:8).
// Used for RT1.a = (clearcoat_factor, clearcoat_roughness).
float PackUnorm8x8(float high, float low)
{
	float h = floor(saturate(high) * 255.0 + 0.5);
	float l = floor(saturate(low) * 255.0 + 0.5);
	return (h * 256.0 + l) / 65535.0;
}

void UnpackUnorm8x8(float packed, out float high, out float low)
{
	float scaled = packed * 65535.0;
	float h = floor(scaled / 256.0);
	high = h / 255.0;
	low = (scaled - h * 256.0) / 255.0;
}

#endif // __NORMAL_HLSL__
