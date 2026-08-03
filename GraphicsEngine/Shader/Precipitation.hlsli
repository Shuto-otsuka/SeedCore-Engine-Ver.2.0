#ifndef __PRECIPITATION_HLSL__
#define __PRECIPITATION_HLSL__

#include "Noise.hlsli"

// Screen-space lightning bolt glow mask, used by the deferred composite pass
// to flash the sky/scene during a Storm's thunder event. Traces a jagged
// polyline from the top of the screen using a seed-derived per-segment
// jitter, then returns an additive glow/core color based on distance from
// the shaded pixel to the nearest segment. `intensity` is expected in
// [0, 1] (0 = no bolt), driven by the weather system's thunder flash curve.
float3 LightningBoltMask(float2 uv, float seed, float intensity)
{
	if (intensity <= 0.0001)
	{
		return float3(0.0, 0.0, 0.0);
	}

	float2 seed_vec = float2(seed, seed * 1.618034);
	float base_x = IQHash(seed_vec) * 0.8 + 0.1;

	const int segment_count = 10;
	float dist_to_bolt = 1e5;
	float x = base_x;
	float y = 0.0;
	for (int segment_index = 0; segment_index < segment_count; ++segment_index)
	{
		float t0 = y;
		float t1 = y + 1.0 / float(segment_count);
		float jitter = (IQHash(seed_vec + float2(float(segment_index), 0.0)) * 2.0 - 1.0) * 0.05;
		float next_x = x + jitter;

		float2 p0 = float2(x, t0);
		float2 p1 = float2(next_x, t1);
		float2 pa = uv - p0;
		float2 ba = p1 - p0;
		float h = saturate(dot(pa, ba) / dot(ba, ba));
		dist_to_bolt = min(dist_to_bolt, length(pa - ba * h));

		x = next_x;
		y = t1;
	}

	float glow = exp(-dist_to_bolt * 220.0) * intensity;
	float core = exp(-dist_to_bolt * 900.0) * intensity;
	float3 bolt_color = float3(0.85, 0.9, 1.0);
	return bolt_color * (glow * 1.5 + core * 4.0);
}

#endif // __PRECIPITATION_HLSL__
