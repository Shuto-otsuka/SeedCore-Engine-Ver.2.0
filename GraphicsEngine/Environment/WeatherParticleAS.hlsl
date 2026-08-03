#include "WeatherParticle.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Culling.hlsli"

// Frustum-culls rain+snow particles (combined dispatch range: [0,
// rain_active_count_) is rain, [rain_active_count_, rain_active_count_ +
// snow_active_count_) is snow - see WeatherParticle.hlsli) and dispatches a
// mesh-shader group per surviving particle, mirroring ImageBillboardAS.hlsl's
// pattern. The payload packs which buffer a survivor came from into its top
// bit (0 = rain, 1 = snow) since both share one dispatch.

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared WeatherParticleASPayload payload;

[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID)
{
	ConstantBuffer<WeatherParticleConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.weather_particle_.ray_constant_index_];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	uint total_active = tuning.rain_active_count_ + tuning.snow_active_count_;
	bool is_visible = false;
	uint packed_index = 0;

	if (dtid.x < total_active)
	{
		bool is_rain = dtid.x < tuning.rain_active_count_;
		uint local_id = is_rain ? dtid.x : (dtid.x - tuning.rain_active_count_);
		packed_index = is_rain ? local_id : (0x80000000u | local_id);

		float3 position;
		if (is_rain)
		{
			StructuredBuffer<WeatherParticle> particles = ResourceDescriptorHeap[structured_indices.weather_particle_.rain_particle_srv_index_];
			position = particles[local_id].position_;
		}
		else
		{
			StructuredBuffer<WeatherParticle> particles = ResourceDescriptorHeap[structured_indices.weather_particle_.snow_particle_srv_index_];
			position = particles[local_id].position_;
		}

		float radius = (is_rain ? tuning.rain_size_ : tuning.snow_size_) * 2.0;
		is_visible = IsVisibleInFrustum(position, radius, scene_constant.current_view_projection_);
	}

	if (is_visible)
	{
		uint slot;
		InterlockedAdd(survived_count, 1, slot);
		local_indices[slot] = packed_index;
	}

	GroupMemoryBarrierWithGroupSync();

	if (gtid.x == 0)
	{
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.particle_indices_[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
