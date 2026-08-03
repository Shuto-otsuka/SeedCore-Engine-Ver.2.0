#ifndef __VOLUMETRIC_STAR_HLSL__
#define __VOLUMETRIC_STAR_HLSL__

// Mirrors GraphicsEngine/Renderer/VolumetricStarRenderer.h's
// VolumetricStarRayConstantBuffer. Laid out as 4-scalar (16-byte) cbuffer
// rows, same convention as VolumetricCloudScapes.hlsli - keep the C++ mirror
// row-for-row in sync.
struct ShootingStarInstance
{
	float3 start_direction_;
	float progress_;   // 0 = just spawned, 1 = reached end_direction_/expired.
	float3 end_direction_;
	float brightness_;
};

struct VolumetricStarRayConstantBuffer
{
	float cell_size_;                    // Angular size (radians) of the star placement grid.
	float density_;                      // 0..1, chance a grid cell holds a star.
	float brightness_;
	float twinkle_speed_;

	float3 color_;
	float size_min_;

	float size_max_;
	float shooting_star_chance_per_second_;
	float shooting_star_brightness_;
	float shooting_star_width_;          // Angular half-width (radians) of a shooting star streak.

	uint max_concurrent_shooting_stars_;
	uint enabled_;                       // Stamped by VolumetricStarRenderer (not the UI), like VolumetricCloudScapes' procedural_sky_enabled_.
	float glow_intensity_;               // Strength of the soft halo outside the SDF star shape.
	float glow_falloff_;                 // How fast the halo fades with SDF distance - higher = tighter glow.

	ShootingStarInstance active_shooting_stars_[4];
};

static const float STAR_PI = 3.14159265358979;
static const float STAR_TWO_PI = 6.28318530717959;

// Builds an orthonormal basis around a unit normal (Duff et al.).
void StarOrthonormalBasis(float3 normal, out float3 tangent, out float3 bitangent)
{
	float s = normal.z >= 0.0 ? 1.0 : -1.0;
	float a = -1.0 / (s + normal.z);
	float b = normal.x * normal.y * a;
	tangent = float3(1.0 + s * normal.x * normal.x * a, s * b, -s * normal.x);
	bitangent = float3(b, s + normal.y * normal.y * a, -normal.y);
}

// GLSL-style mod (always non-negative for a positive y), as opposed to
// HLSL's fmod which keeps the sign of x - StarSDF's angular folding needs
// the non-negative form.
float GlslMod(float x, float y)
{
	return x - y * floor(x / y);
}

// General N-pointed star SDF (Inigo Quilez, "2D distance to star"). p is in
// the shape's local space, centered on the star. r is the outer (tip)
// radius, n the point count, m the spike sharpness (roughly in (2, n) -
// closer to n gives fatter points, closer to 2 gives thin needles).
// Negative inside the star, positive outside, zero on the outline - exact
// distance, so a glow can just be a function of the returned value.
float StarSDF(float2 p, float r, float n, float m)
{
	float an = STAR_PI / n;
	float en = STAR_PI / m;
	float2 acs = float2(cos(an), sin(an));
	float2 ecs = float2(cos(en), sin(en));

	float bn = GlslMod(atan2(p.x, p.y), 2.0 * an) - an;
	p = length(p) * float2(cos(bn), abs(sin(bn)));
	p -= r * acs;
	p += ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);
	return length(p) * sign(p.x);
}

// Procedural night sky: a latitude/longitude grid over the view direction,
// one hashed star per occupied cell (checking the 3x3 neighborhood so a star
// centered near a cell edge is not clipped). Distortion near the poles from
// the lat/long parameterization is an accepted simplification - not
// physically accurate, but stable (no seams) and cheap (no precomputed buffer).
// Each star is a proper 5/6-point StarSDF shape (small, since real stars read
// as pinpoints) plus a soft exponential glow so it still reads at a distance;
// about 1/30 of stars get a distinct red/blue/green tint (like Betelgeuse/
// Rigel) instead of the base tuning.color_, echoing how bright real stars
// show visible color from their temperature.
float3 StarFieldColor(float3 view_direction, float night_factor, float total_time, VolumetricStarRayConstantBuffer tuning)
{
	if (night_factor <= 0.0 || tuning.enabled_ == 0)
	{
		return float3(0, 0, 0);
	}

	float cell_size = max(tuning.cell_size_, 0.001);

	float theta = acos(clamp(view_direction.y, -1.0, 1.0));
	float phi = atan2(view_direction.z, view_direction.x);
	phi = phi < 0.0 ? phi + STAR_TWO_PI : phi;

	float2 grid = float2(phi, theta) / cell_size;
	float2 cell = floor(grid);

	float3 sum = float3(0, 0, 0);

	for (int dy = -1; dy <= 1; dy++)
	{
		for (int dx = -1; dx <= 1; dx++)
		{
			float2 neighbor_cell = cell + float2(dx, dy);
			float4 h = IQHash4(float3(neighbor_cell, 7.0));

			if (h.x > tuning.density_)
			{
				continue;
			}

			float2 star_center = neighbor_cell + float2(0.5, 0.5) + (h.yz - 0.5) * 0.6;
			float2 delta = grid - star_center;

			float point_count = h.w < 0.5 ? 5.0 : 6.0;
			float spike_sharpness = point_count - 2.0;
			float size = lerp(tuning.size_min_, tuning.size_max_, frac(h.x * 37.0));

			float sdf_distance = StarSDF(delta, size, point_count, spike_sharpness);

			// Sharp-ish core fill for the SDF shape itself, plus a soft
			// exponential halo outside it - the actual "Glow".
			float core = smoothstep(0.0, -size * 0.15, sdf_distance);
			float glow = exp(-max(sdf_distance, 0.0) * tuning.glow_falloff_) * tuning.glow_intensity_;
			float shape = max(core, glow);

			float twinkle = 0.6 + 0.4 * sin(total_time * tuning.twinkle_speed_ + h.x * 123.4);

			// ~1/30 of stars take a distinct color instead of tuning.color_,
			// like bright real stars showing their temperature (Rigel=blue,
			// Betelgeuse=red).
			float4 tint_hash = IQHash4(float3(neighbor_cell, 13.0));
			float3 star_color = tuning.color_;
			if (tint_hash.x < (1.0 / 30.0))
			{
				if (tint_hash.y < 0.333)
				{
					star_color = float3(1.0, 0.35, 0.3);
				}
				else if (tint_hash.y < 0.666)
				{
					star_color = float3(0.55, 0.75, 1.0);
				}
				else
				{
					star_color = float3(0.5, 1.0, 0.6);
				}
			}

			sum += star_color * shape * twinkle * tuning.brightness_;
		}
	}

	return sum * night_factor;
}

// Moon disc with a phase-shaped terminator. phase: 0 = new moon (dark),
// 0.5 = full moon (fully lit), 1 = new moon again. Uses the classic
// "two overlapping circles" technique: an elliptical terminator curve inside
// the disc's local 2D space, its horizontal offset driven by the illuminated
// fraction derived from phase.
float3 MoonDiscColor(float3 view_direction, float3 moon_direction, float moon_radius, float3 moon_color, float phase)
{
	float cos_angle = dot(view_direction, moon_direction);
	float cos_disc = cos(moon_radius);

	if (cos_angle < cos_disc)
	{
		return float3(0, 0, 0);
	}

	float3 tangent, bitangent;
	StarOrthonormalBasis(moon_direction, tangent, bitangent);

	float3 offset = view_direction - moon_direction * cos_angle;
	float2 local = float2(dot(offset, tangent), dot(offset, bitangent)) / max(sin(moon_radius), 0.0001);

	// Illuminated fraction 0..1 from phase (0/1 = new, 0.5 = full).
	float illuminated = 0.5 - 0.5 * cos(phase * STAR_TWO_PI);

	// shift 1 = terminator at the disc's own edge (nothing lit), -1 = at the
	// opposite edge (everything lit).
	float shift = 1.0 - 2.0 * illuminated;

	float terminator_x = shift * sqrt(saturate(1.0 - local.y * local.y));
	bool waxing = phase < 0.5;
	bool lit = waxing ? (local.x > terminator_x) : (local.x < -terminator_x);

	if (!lit)
	{
		return float3(0, 0, 0);
	}

	float disc_mask = saturate(1.0 - length(local));
	return moon_color * smoothstep(0.0, 0.15, disc_mask + 0.85);
}

// Additive streaks for the active shooting star slots. Each streak is the
// great-circle arc segment between its trailing edge (progress_ - trail
// length) and its head (progress_), brightest at the head like a comet.
float3 ShootingStarColor(float3 view_direction, VolumetricStarRayConstantBuffer tuning)
{
	float3 total = float3(0, 0, 0);

	const float trail_length = 0.12;

	for (uint index = 0; index < tuning.max_concurrent_shooting_stars_; index++)
	{
		ShootingStarInstance instance = tuning.active_shooting_stars_[index];

		if (instance.progress_ <= 0.0 || instance.progress_ >= 1.0)
		{
			continue;
		}

		float3 start_direction = normalize(instance.start_direction_);
		float3 end_direction = normalize(instance.end_direction_);

		float3 head_direction = normalize(lerp(start_direction, end_direction, instance.progress_));
		float3 tail_direction = normalize(lerp(start_direction, end_direction, max(instance.progress_ - trail_length, 0.0)));

		float3 arc_normal = cross(head_direction, tail_direction);
		float arc_normal_length = length(arc_normal);
		if (arc_normal_length < 0.0001)
		{
			continue;
		}
		arc_normal /= arc_normal_length;

		float perpendicular_angle = asin(clamp(dot(view_direction, arc_normal), -1.0, 1.0));
		float perpendicular_glow = saturate(1.0 - abs(perpendicular_angle) / max(tuning.shooting_star_width_, 0.0001));

		if (perpendicular_glow <= 0.0)
		{
			continue;
		}

		float3 in_plane = normalize(view_direction - arc_normal * dot(view_direction, arc_normal));
		float total_angle = acos(clamp(dot(head_direction, tail_direction), -1.0, 1.0));
		float angle_from_tail = acos(clamp(dot(in_plane, tail_direction), -1.0, 1.0));
		float t = total_angle > 0.0001 ? saturate(angle_from_tail / total_angle) : 1.0;

		float head_angle = acos(clamp(dot(view_direction, head_direction), -1.0, 1.0));
		float head_glow = saturate(1.0 - head_angle / max(tuning.shooting_star_width_ * 3.0, 0.0001));

		float streak = perpendicular_glow * t;
		float brightness = max(streak, head_glow) * instance.brightness_;

		total += tuning.color_ * tuning.shooting_star_brightness_ * brightness;
	}

	return total;
}

#endif // __VOLUMETRIC_STAR_HLSL__
