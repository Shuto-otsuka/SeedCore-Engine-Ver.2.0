#ifndef __PARTICLE_HLSL__
#define __PARTICLE_HLSL__

#include "../../Shader/Dispatch.hlsli"

struct ParticleSeed
{
	float3 position_;
	float3 velocity_;
	float4 color_;
	float2 size_;
	float rotation_;
	float age_;
	float lifetime_;
	uint seed_;
	uint emitter_id_;
};

struct ParticleMeta
{
	float emitter_delta_;
	float emitter_age_;
	uint sim_space_;
	float4x4 emitter_to_world_;
	float4x4 world_to_emitter_;
};

ConstantBuffer<ParticleMeta> GetParticleMeta()
{
	ParticleDispatchBuffer dispatchBuffer = GetParticleDispatchBuffer();
	return ResourceDescriptorHeap[dispatchBuffer.meta_index_];
}

#endif // __PARTICLE_HLSL__
