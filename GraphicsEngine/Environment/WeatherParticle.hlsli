#ifndef __WEATHER_PARTICLE_HLSL__
#define __WEATHER_PARTICLE_HLSL__

// Shared by WeatherParticleSimulateCS/AS/MS/PS.hlsl. Mirrors
// GraphicsEngine/Renderer/WeatherParticleRenderer.h's WeatherParticle
// byte-for-byte - keep both in sync.
struct WeatherParticle
{
	float3 position_;
	float life_;        // 0..1, fraction of its fall already completed.
	float3 velocity_;
	float seed_;         // per-particle random, used for size/twinkle/sway variance.
};

// One combined tuning buffer for BOTH the rain and snow particle systems -
// rain_/snow_ prefixed fields, plus shared fields (camera, time, wind). A
// single buffer (rather than one each) lets WeatherParticleSimulateCS/AS
// handle both particle sets in one dispatch by splitting the thread/group
// range: [0, rain_capacity_) is rain, [rain_capacity_, rain_capacity_ +
// snow_capacity_) is snow - there is no spare root-signature slot to swap a
// per-dispatch tuning CBV, so this is how one compiled shader serves both
// without duplicating files. Mirrors WeatherParticleRenderer.h's
// WeatherParticleConstantBuffer byte-for-byte.
struct WeatherParticleConstantBuffer
{
	float3 camera_position_;
	float delta_time_;

	float3 wind_;
	float total_time_;

	uint force_respawn_;     // 1 on the first frame - unconditionally respawns every particle.
	uint rain_capacity_;     // fixed pool size (buffer element count).
	uint rain_active_count_; // how many of the pool are actually simulated/drawn this frame.
	float rain_fall_speed_;

	float rain_size_;
	float rain_streak_length_;
	float rain_brightness_;
	float rain_volume_radius_; // camera-relative spawn/kill cylinder radius (XZ).

	float rain_volume_height_; // camera-relative spawn/kill cylinder half-height (Y), spawns at the top.
	float3 rain_color_;

	uint snow_capacity_;
	uint snow_active_count_;
	float snow_fall_speed_;
	float snow_sway_amount_;

	float snow_size_;
	float snow_brightness_;
	float snow_volume_radius_;
	float snow_volume_height_;

	float3 snow_color_;
	float weather_particle_padding0_;
};

// Amplification shader payload: which particle indices (from the culled
// dispatch, in the combined [0, rain_capacity_+snow_capacity_) range) each
// mesh-shader group should expand.
struct WeatherParticleASPayload
{
	uint particle_indices_[32];
};

struct WeatherParticleMSOutput
{
	float4 position_ : SV_Position;
	float2 local_     : TEXCOORD0;
	float3 color_     : COLOR0;
	nointerpolation uint isRain_ : BLENDINDICES;
};

#endif // __WEATHER_PARTICLE_HLSL__
