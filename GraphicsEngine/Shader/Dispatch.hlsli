#ifndef __DISPATCH_HLSL__
#define __DISPATCH_HLSL__

cbuffer DispatchIndices : register(b2, space1)
{
	uint dispatch_buffer_index_;
};

struct ParticleDispatchBuffer
{
	uint particle_index_;
	uint dead_list_index_;
	uint alive_list_read_index_;
	uint alive_list_write_index_;

	uint counter_index_;
	uint module_index_;
	uint meta_index_;
	uint particle_dispatch_buffer_0_padding_;
};

ConstantBuffer<ParticleDispatchBuffer> GetParticleDispatchBuffer()
{
	return ResourceDescriptorHeap[dispatch_buffer_index_];
}

#endif // __DISPATCH_HLSL__
