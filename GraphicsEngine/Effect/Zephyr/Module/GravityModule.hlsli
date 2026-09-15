#ifndef __GRAVITY_MODULE_HLSL__
#define __GRAVITY_MODULE_HLSL__

#include "../Particle.hlsli"

struct GravityModule
{
	float3 gravity_;
};

void ApplyGravityUpdate(inout ParticleSeed seed, GravityModule module_, ParticleMeta meta)
{
	seed.velocity_ += module_.gravity_ * meta.emitter_delta_;
}

#endif // __GRAVITY_MODULE_HLSL__
