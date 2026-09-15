#ifndef __PARTICLE_POOL_HLSL__
#define __PARTICLE_POOL_HLSL__

#include "../../Shader/Dispatch.hlsli"
#include "Particle.hlsli"

struct ParticleCountersConstantBuffer
{
	uint alive_count_;
	uint dead_count_;
	uint alive_write_count_;
	uint particle_counters_padding_0_;
};

RWStructuredBuffer<ParticleSeed> GetParticleBuffer()
{
	ParticleDispatchBuffer dispatchBuffer = GetParticleDispatchBuffer();
	return ResourceDescriptorHeap[dispatchBuffer.particle_index_];
}

RWStructuredBuffer<uint> GetDeadList()
{
	ParticleDispatchBuffer dispatchBuffer = GetParticleDispatchBuffer();
	return ResourceDescriptorHeap[dispatchBuffer.dead_list_index_];
}

RWStructuredBuffer<uint> GetAliveListRead()
{
	ParticleDispatchBuffer dispatchBuffer = GetParticleDispatchBuffer();
	return ResourceDescriptorHeap[dispatchBuffer.alive_list_read_index_];
}

RWStructuredBuffer<uint> GetAliveListWrite()
{
	ParticleDispatchBuffer dispatchBuffer = GetParticleDispatchBuffer();
	return ResourceDescriptorHeap[dispatchBuffer.alive_list_write_index_];
}

RWStructuredBuffer<ParticleCountersConstantBuffer> GetParticleCountersConstantBuffer()
{
	ParticleDispatchBuffer dispatchBuffer = GetParticleDispatchBuffer();
	return ResourceDescriptorHeap[dispatchBuffer.counter_index_];
}

#endif // __PARTICLE_POOL_HLSL__
