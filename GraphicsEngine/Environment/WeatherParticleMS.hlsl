#include "WeatherParticle.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"

// Expands one particle (decoded from the AS payload's packed rain/snow +
// local index, see WeatherParticleAS.hlsl) into a camera-facing quad. Rain
// additionally aligns the quad's long axis with its velocity direction and
// stretches it (a motion-blur streak, sized by speed) instead of the plain
// square billboard snow uses.

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload WeatherParticleASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices WeatherParticleMSOutput output[4], out indices uint3 triangles[2])
{
	ConstantBuffer<WeatherParticleConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.weather_particle_.ray_constant_index_];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();

	SetMeshOutputCounts(4u, 2u);

	uint packed = payload.particle_indices_[gid];
	bool is_rain = (packed & 0x80000000u) == 0;
	uint local_id = packed & 0x7fffffffu;

	WeatherParticle particle;
	float size;
	float brightness;
	float3 color;
	float streak_length;

	if (is_rain)
	{
		StructuredBuffer<WeatherParticle> particles = ResourceDescriptorHeap[structured_indices.weather_particle_.rain_particle_srv_index_];
		particle = particles[local_id];
		size = tuning.rain_size_;
		brightness = tuning.rain_brightness_;
		color = tuning.rain_color_;
		streak_length = tuning.rain_streak_length_;
	}
	else
	{
		StructuredBuffer<WeatherParticle> particles = ResourceDescriptorHeap[structured_indices.weather_particle_.snow_particle_srv_index_];
		particle = particles[local_id];
		size = tuning.snow_size_;
		brightness = tuning.snow_brightness_;
		color = tuning.snow_color_;
		streak_length = 0.0;
	}

	float3 camera_right = scene_constant.inverse_view_[0].xyz;
	float3 camera_up = scene_constant.inverse_view_[1].xyz;

	float3 quad_up = camera_up;
	float half_length = size * 0.5;

	if (streak_length > 0.0)
	{
		float speed = length(particle.velocity_);
		if (speed > 0.0001)
		{
			// Align the quad's long axis with the direction of travel and
			// stretch it by speed - this is what reads as "falling fast due
			// to gravity" instead of a static dot merely sliding in place.
			quad_up = particle.velocity_ / speed;
			half_length = max(size * 0.5, speed * streak_length * 0.5);
		}
	}

	float3 view_direction = normalize(scene_constant.camera_position_.xyz - particle.position_);
	float3 quad_right = cross(quad_up, view_direction);
	float quad_right_length = length(quad_right);
	quad_right = quad_right_length > 0.0001 ? (quad_right / quad_right_length) : camera_right;

	float2 offsets[4] =
	{
		float2(-1.0,  1.0),
		float2( 1.0,  1.0),
		float2(-1.0, -1.0),
		float2( 1.0, -1.0)
	};

	float fade = saturate(particle.life_);

	if (gtid < 4u)
	{
		float3 world_position = particle.position_ + quad_right * (offsets[gtid].x * size * 0.5) + quad_up * (offsets[gtid].y * half_length);
		output[gtid].position_ = mul(float4(world_position, 1.0), scene_constant.current_view_projection_);
		output[gtid].local_ = offsets[gtid];
		output[gtid].color_ = color * brightness * fade;
		output[gtid].isRain_ = is_rain ? 1u : 0u;
	}

	if (gtid == 0)
	{
		triangles[0] = uint3(0u, 1u, 2u);
		triangles[1] = uint3(1u, 3u, 2u);
	}
}
