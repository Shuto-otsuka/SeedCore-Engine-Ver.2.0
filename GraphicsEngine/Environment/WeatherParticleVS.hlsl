#include "WeatherParticle.hlsli"
#include "../Shader/ShaderResources.hlsli"
#include "../Shader/Constants.hlsli"
#include "../Shader/Culling.hlsli"

WeatherParticleMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	ConstantBuffer<WeatherParticleConstantBuffer> tuning = ResourceDescriptorHeap[constant_indices.weather_particle_index_];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();

	WeatherParticleMSOutput output = (WeatherParticleMSOutput)0;
	output.position_ = float4(2.0f, 2.0f, 2.0f, 1.0f);

	uint total_active = tuning.rain_active_count_ + tuning.snow_active_count_;
	if (instance_id >= total_active)
	{
		return output;
	}

	bool is_rain = instance_id < tuning.rain_active_count_;
	uint local_id = is_rain ? instance_id : (instance_id - tuning.rain_active_count_);

	WeatherParticle particle;
	float size;
	float brightness;
	float3 color;
	float streak_length;

	if (is_rain)
	{
		StructuredBuffer<WeatherParticle> particles = ResourceDescriptorHeap[shader_resource_indices.weather_particle_.rain_particle_index_];
		particle = particles[local_id];
		size = tuning.rain_size_;
		brightness = tuning.rain_brightness_;
		color = tuning.rain_color_;
		streak_length = tuning.rain_streak_length_;
	}
	else
	{
		StructuredBuffer<WeatherParticle> particles = ResourceDescriptorHeap[shader_resource_indices.weather_particle_.snow_particle_index_];
		particle = particles[local_id];
		size = tuning.snow_size_;
		brightness = tuning.snow_brightness_;
		color = tuning.snow_color_;
		streak_length = 0.0;
	}

	float radius = size * 2.0;
	if (!IsVisibleInFrustum(particle.position_, radius, scene_constant.current_view_projection_))
	{
		return output;
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
			quad_up = particle.velocity_ / speed;
			half_length = max(size * 0.5, speed * streak_length * 0.5);
		}
	}

	float3 view_direction = normalize(scene_constant.camera_position_.xyz - particle.position_);
	float3 quad_right = cross(quad_up, view_direction);
	float quad_right_length = length(quad_right);
	quad_right = quad_right_length > 0.0001 ? (quad_right / quad_right_length) : camera_right;

	uint quad_indices[6] = { 0u, 1u, 2u, 1u, 3u, 2u };
	uint corner_index = quad_indices[vertex_id];

	float2 offsets[4] =
	{
		float2(-1.0,  1.0),
		float2( 1.0,  1.0),
		float2(-1.0, -1.0),
		float2( 1.0, -1.0)
	};

	float fade = saturate(particle.life_);

	float3 world_position = particle.position_ + quad_right * (offsets[corner_index].x * size * 0.5) + quad_up * (offsets[corner_index].y * half_length);
	output.position_ = mul(float4(world_position, 1.0), scene_constant.current_view_projection_);
	output.local_ = offsets[corner_index];
	output.color_ = color * brightness * fade;
	output.isRain_ = is_rain ? 1u : 0u;

	return output;
}
