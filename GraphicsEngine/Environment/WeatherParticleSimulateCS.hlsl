#include "../Shader/Constants.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Noise.hlsli"
#include "WeatherParticle.hlsli"

/**
* [EN]
* Advances every rain and snow particle (one combined dispatch, see
* WeatherParticle.hlsli for why they share a buffer range instead of two
* separate dispatches). No depth/geometry awareness here at all - that
* "collision" is handled for free, per-pixel, by the hardware depth test in
* WeatherParticleMS/PS.hlsl's draw pass. This pass only does the physics
* (constant fall velocity + wind) and recycling: a particle whose
* camera-relative position falls outside a cylinder around the camera (below
* by volume_height, or beyond volume_radius horizontally) is respawned at the
* top of that cylinder with a fresh random XZ offset - an "infinite rain"
* volume that always surrounds wherever the camera currently is.
*
* [JP]
* 雨と雪の全パーティクルを進める(1回のディスパッチにまとめる理由は
* WeatherParticle.hlsli 参照 - 別ディスパッチにしない)。深度/ジオメトリの
* 認識はここには一切無い - その「衝突」は WeatherParticleMS/PS.hlsl の
* 描画パスでハードウェア深度テストが画素単位でタダで処理する。このパスが
* 行うのは物理(一定落下速度+風)と再スポーンだけ: カメラ相対位置が
* 円柱(下方向は volume_height、水平方向は volume_radius)の外に出た
* パーティクルは、円柱の天井へランダムなXZオフセット付きで再スポーンする
* - カメラが今どこにいてもその周りを覆う「無限に降る」ボリューム。
*/

void RespawnParticle(inout WeatherParticle particle, uint globalId, float3 cameraPosition, float volumeRadius, float volumeHeight, float3 baseVelocity, float3 wind, float seedOffset, bool randomizeHeight)
{
	// Hashing only (globalId, seedOffset) would give every particle the exact
	// same respawn position/seed on every single recycle - a fixed point per
	// index, which reads as "the same fall pattern looping" once enough
	// particles have cycled through. Chaining off the particle's own previous
	// seed_ (itself the output of the last respawn's hash) turns this into an
	// evolving per-particle RNG sequence instead of a constant.
	float3 rand = IQHash3(float3(float(globalId) + particle.seed_ * 137.51, seedOffset + particle.seed_ * 251.73, particle.seed_ * 71.31 + 1.0));
	float angle = rand.x * 6.28318530718;
	float radius = sqrt(rand.y) * volumeRadius;

	// The very first (force_respawn_) spawn scatters particles across the
	// whole cylinder height instead of stacking them all at the ceiling -
	// otherwise every particle falls in one synchronized sheet forever,
	// since normal recycling only ever refills the ceiling one at a time.
	float height = randomizeHeight ? (rand.z * 2.0 - 1.0) * volumeHeight : volumeHeight;

	particle.position_ = cameraPosition + float3(cos(angle) * radius, height, sin(angle) * radius);
	particle.velocity_ = baseVelocity + wind;
	particle.life_ = 0.0;
	particle.seed_ = rand.z;
}

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	ConstantBuffer<WeatherParticleConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.weather_particle_.ray_constant_index_];

	uint id = dtid.x;
	uint totalCapacity = tuning.rain_capacity_ + tuning.snow_capacity_;
	if (id >= totalCapacity)
	{
		return;
	}

	const float fadeInTime = 0.25;

	if (id < tuning.rain_capacity_)
	{
		RWStructuredBuffer<WeatherParticle> particles = ResourceDescriptorHeap[structured_indices.weather_particle_.rain_particle_uav_index_];
		uint localId = id;
		WeatherParticle particle = particles[localId];

		float3 relative = particle.position_ - tuning.camera_position_;
		bool outOfBounds = (relative.y < -tuning.rain_volume_height_) || (dot(relative.xz, relative.xz) > tuning.rain_volume_radius_ * tuning.rain_volume_radius_);

		if (tuning.force_respawn_ != 0 || outOfBounds)
		{
			float3 baseVelocity = float3(0.0, -tuning.rain_fall_speed_, 0.0);
			RespawnParticle(particle, localId, tuning.camera_position_, tuning.rain_volume_radius_, tuning.rain_volume_height_, baseVelocity, tuning.wind_, 11.0, tuning.force_respawn_ != 0);
		}
		else
		{
			particle.position_ += particle.velocity_ * tuning.delta_time_;
			particle.life_ = min(particle.life_ + tuning.delta_time_ / fadeInTime, 1.0);
		}

		particles[localId] = particle;
	}
	else
	{
		RWStructuredBuffer<WeatherParticle> particles = ResourceDescriptorHeap[structured_indices.weather_particle_.snow_particle_uav_index_];
		uint localId = id - tuning.rain_capacity_;
		WeatherParticle particle = particles[localId];

		float3 relative = particle.position_ - tuning.camera_position_;
		bool outOfBounds = (relative.y < -tuning.snow_volume_height_) || (dot(relative.xz, relative.xz) > tuning.snow_volume_radius_ * tuning.snow_volume_radius_);

		if (tuning.force_respawn_ != 0 || outOfBounds)
		{
			float3 baseVelocity = float3(0.0, -tuning.snow_fall_speed_, 0.0);
			RespawnParticle(particle, localId, tuning.camera_position_, tuning.snow_volume_radius_, tuning.snow_volume_height_, baseVelocity, tuning.wind_ * 0.3, 37.0, tuning.force_respawn_ != 0);
		}
		else
		{
			// Gentle per-particle sway, phase derived from its own seed (not
			// global time alone) so flakes desync instead of swaying in lockstep.
			float swayPhase = particle.seed_ * 6.28318530718;
			float3 sway = float3(sin(tuning.total_time_ * 0.8 + swayPhase), 0.0, cos(tuning.total_time_ * 0.6 + swayPhase)) * tuning.snow_sway_amount_;
			particle.position_ += (particle.velocity_ + sway) * tuning.delta_time_;
			particle.life_ = min(particle.life_ + tuning.delta_time_ / fadeInTime, 1.0);
		}

		particles[localId] = particle;
	}
}
