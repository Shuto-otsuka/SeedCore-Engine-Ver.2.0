#ifndef __VOLUMETRIC_CLOUD_SCAPES_HLSL__
#define __VOLUMETRIC_CLOUD_SCAPES_HLSL__

static const float CLOUD_PI = 3.14159265358979;

/**
* Sky + cloud tuning constant buffer, read by VolumetricCloudScapesRT.hlsl,
* DeferredLightingPS.hlsl, ProceduralSkyToCubeCS.hlsl and
* VolumetricLightScatteringRT.hlsl via
* structured_indices.cloud_.ray_constant_index_.
* Must match the C++ mirror in Renderer/VolumetricCloudScapesRenderer.h
* byte-for-byte. Every group of four scalars below is one 16-byte cbuffer
* row - keep it that way when adding fields.
*/
struct VolumetricCloudScapesRayConstantBuffer
{
	/// Cloud layer altitudes, measured above the CURVED ground (see
	/// CloudAltitudeAt) rather than as an infinite flat slab in world Y.
	float cloud_bottom_;
	float cloud_top_;

	/// Cloud amount 0..1.
	float coverage_;

	/// Extinction scale of the density field.
	float density_scale_;

	float3 cloud_albedo_;

	/// Henyey-Greenstein anisotropy of the FORWARD lobe (0 = isotropic,
	/// ~0.6 = forward). See phase_backward_ / phase_blend_ for the second lobe.
	float scattering_g_;

	/// View sample budget (the marcher spends it adaptively, see
	/// VolumetricCloudScapesRT.hlsl) / sun lightmarch steps.
	uint step_count_;
	uint light_step_count_;

	/// World position -> shape noise domain scale (smaller = bigger shapes).
	float noise_scale_;

	/// Wind scroll speed (noise domain units per second).
	float wind_speed_;

	/// Per-frame counter. Only used for the raymarch start jitter, and only
	/// when temporal_jitter_ is non-zero.
	uint frame_index_;

	/// 0 = stratus (flat, low band), 1 = cumulus (tall, billowing).
	/// Interpolates the height-density profile.
	float cloud_type_;

	/// 0 = fair weather, 1 = rain clouds: darkens the albedo, raises
	/// coverage/density.
	float rain_;

	/// Detail noise domain scale relative to the shape scale.
	float detail_scale_;

	/// ---- Procedural sky (used only when no skymap is bound) ----

	/// Sky gradient: zenith (straight up - "how blue the sky is") and
	/// horizon colors.
	float3 sky_zenith_color_;

	/// Angular radius of the sun disc (radians).
	float sun_size_;

	float3 sky_horizon_color_;

	/// Overall sky brightness multiplier.
	float sky_brightness_;

	/// Below-horizon fill color.
	float3 ground_color_;

	/// 1 while the cloud/sky feature is enabled (set by the renderer, not the
	/// UI) - DeferredLightingPS.hlsl uses this to decide whether to draw the
	/// procedural sky on skymap-less backgrounds.
	uint procedural_sky_enabled_;

	/// ---- Layer geometry ----

	/// Planet radius in world units. The cloud layer is a shell around a
	/// sphere of this radius, so the layer bends down and converges at the
	/// horizon. Larger = flatter.
	float planet_radius_;

	/// Far limit of the cloud raymarch. The layer's own horizon distance is
	/// sqrt(2 * planet_radius_ * cloud_bottom_), which is far beyond any
	/// sensible march - alpha is faded out toward this limit so the cut is
	/// invisible.
	float max_march_distance_;

	/// Distance at which step length starts growing and detail noise starts
	/// fading (the volumes have no mip chain, so this stands in for LOD).
	float lod_distance_;

	/// Aerial perspective: 1/e distance for washing distant clouds toward the
	/// horizon sky color.
	float aerial_density_;

	/// ---- Weather ----

	/// Weather field frequency, RELATIVE to noise_scale_. Smaller = larger
	/// cloud masses.
	float weather_scale_;

	/// How hard the weather field pushes coverage around, 0..1. 0 restores the
	/// old globally-uniform coverage.
	float weather_amount_;

	/// Detail erosion amount (how much the high-frequency volume eats into the
	/// cloud edges).
	float detail_strength_;

	/// Detail scroll speed relative to wind_speed_. The detail volume needs its
	/// own offset - see CloudDensity.
	float detail_wind_speed_;

	/// ---- Phase / scattering ----

	/// Backward lobe anisotropy (positive value, negated internally) and the
	/// blend weight between the forward and backward lobes.
	float phase_backward_;
	float phase_blend_;

	/// Beer-powder strength, 0..1: darkens the sunlit surface of a cloud.
	float powder_strength_;

	/// Sky-gradient ambient multiplier (keeps cloud undersides from crushing
	/// to black).
	float ambient_strength_;

	/// ---- Multiple scattering approximation ----

	/// Octave count, and the per-octave scales for extinction, contribution
	/// and phase eccentricity (0.5 each is the usual choice).
	uint multi_scatter_octaves_;
	float multi_scatter_attenuation_;
	float multi_scatter_contribution_;
	float multi_scatter_eccentricity_;

	/// 0 = frame-independent dither (stable, no crawling grain).
	/// 1 = full per-frame rotation, for use behind TAA/DLSS.
	float temporal_jitter_;

	float cloud_padding_0_;
	float cloud_padding_1_;
	float cloud_padding_2_;
};

// ---------------------------------------------------------------------------
// Layer geometry
// ---------------------------------------------------------------------------

/**
* Curvature term for a ray: altitude(t) = origin.y + direction.y * t + k*t*t.
*
* A ray leaving a curved planet gains altitude quadratically as the surface
* falls away from it (the sagitta d^2 / 2R). Using this parabolic form instead
* of an exact ray/sphere intersection is deliberate: with a radius around
* 6.4e6 world units, |origin - center|^2 - radius^2 loses every significant
* bit to cancellation in fp32. Every quantity here stays small and
* well-conditioned, and the approximation error over a 120km march is far
* below a world unit.
*/
float CloudCurvature(float3 direction, float planet_radius)
{
	float horizontal_squared = saturate(1.0 - direction.y * direction.y);
	return horizontal_squared / (2.0 * max(planet_radius, 1.0));
}

/// Altitude above the curved ground at parametric distance t along a ray.
float CloudAltitudeAt(float origin_height, float direction_y, float curvature, float t)
{
	return origin_height + direction_y * t + curvature * t * t;
}

/**
* Solves altitude(t) == target, returning both roots ordered near/far.
* Returns false when the ray never reaches that altitude at all.
*
* Uses the numerically stable quadratic form (q = -0.5*(b + sign(b)*sqrt(D)),
* roots q/a and c/q) because 4*k*c is many orders of magnitude below b*b for
* shallow rays, which is exactly where the naive (-b +- sqrt(D)) / 2a form
* cancels away its own precision.
*/
bool CloudSolveAltitude(float origin_height, float direction_y, float curvature, float target, out float near_t, out float far_t)
{
	near_t = 0.0;
	far_t = 0.0;

	/// Near-vertical rays have (almost) no horizontal extent, so the quadratic
	/// degenerates to the flat-slab linear solve.
	if (curvature < 1e-12)
	{
		if (abs(direction_y) < 1e-6)
		{
			return false;
		}

		float linear_t = (target - origin_height) / direction_y;
		near_t = linear_t;
		far_t = linear_t;
		return linear_t > 0.0;
	}

	float c = origin_height - target;
	float discriminant = direction_y * direction_y - 4.0 * curvature * c;

	if (discriminant < 0.0)
	{
		return false;
	}

	float root = sqrt(discriminant);
	float q = -0.5 * (direction_y + (direction_y >= 0.0 ? root : -root));

	float t0 = q / curvature;
	float t1 = abs(q) > 1e-20 ? c / q : t0;

	near_t = min(t0, t1);
	far_t = max(t0, t1);
	return true;
}

/**
* t-range over which a ray is inside the cloud shell, clamped to the ground
* and to max_march_distance_. Returns false when the ray misses the layer.
*
* Handles all four camera cases (below / inside / above the layer, and looking
* down into the ground). The important consequence of the curved solve is that
* a horizontal ray gets a FINITE entry distance - the layer's horizon - rather
* than the infinite flat slab's "clouds at full thickness forever", which is
* what produced the hard straight cut across the sky.
*/
bool CloudLayerInterval(float3 origin, float3 direction, VolumetricCloudScapesRayConstantBuffer tuning, out float t_enter, out float t_exit)
{
	t_enter = 0.0;
	t_exit = 0.0;

	float curvature = CloudCurvature(direction, tuning.planet_radius_);
	float height = origin.y;

	/// The ground caps the march, but must not reject the ray outright: from
	/// above the layer, looking down, the clouds come before the ground does.
	float ground_limit = tuning.max_march_distance_;
	float ground_near;
	float ground_far;
	if (CloudSolveAltitude(height, direction.y, curvature, 0.0, ground_near, ground_far) && ground_near > 0.0)
	{
		ground_limit = ground_near;
	}

	float bottom_near;
	float bottom_far;
	float top_near;
	float top_far;
	bool hits_bottom = CloudSolveAltitude(height, direction.y, curvature, tuning.cloud_bottom_, bottom_near, bottom_far);
	bool hits_top = CloudSolveAltitude(height, direction.y, curvature, tuning.cloud_top_, top_near, top_far);

	if (height < tuning.cloud_bottom_)
	{
		/// Below the layer: altitude rises monotonically on the outbound
		/// branch, so both crossings are the far roots.
		if (!hits_bottom || !hits_top)
		{
			return false;
		}
		t_enter = max(bottom_far, 0.0);
		t_exit = max(top_far, 0.0);
	}
	else if (height > tuning.cloud_top_)
	{
		/// Above the layer: descend through the top, then the bottom.
		if (!hits_top)
		{
			return false;
		}
		t_enter = max(top_near, 0.0);
		t_exit = hits_bottom ? max(bottom_near, 0.0) : max(top_far, 0.0);
	}
	else
	{
		/// Inside the layer.
		t_enter = 0.0;
		if (direction.y >= 0.0)
		{
			t_exit = hits_top ? top_far : tuning.max_march_distance_;
		}
		else
		{
			t_exit = hits_bottom ? bottom_near : (hits_top ? top_far : tuning.max_march_distance_);
		}
	}

	float far_limit = min(ground_limit, tuning.max_march_distance_);
	t_exit = min(t_exit, far_limit);

	return t_exit > t_enter && t_enter < far_limit;
}

// ---------------------------------------------------------------------------
// Density field
// ---------------------------------------------------------------------------

/**
* Cloud density field: world position -> extinction. Shared by the screen-space
* raymarch (VolumetricCloudScapesRT.hlsl), the environment cube bake
* (ProceduralSkyToCubeCS.hlsl - that's how clouds reach IBL and reflections)
* and the godray pass (VolumetricLightScatteringRT.hlsl). The noise volumes are
* the pre-baked tileable Texture3Ds, wrap-sampled.
*
* altitude is the sample's height above the CURVED ground, computed by the
* caller along its own ray (CloudAltitudeAt) - passing it in rather than
* deriving it from world_position.y keeps the curvature reference tied to the
* ray's own origin instead of the world origin.
*
* detail_strength scales the high-frequency erosion. Callers pass 0 for the
* cheap empty-space-skipping and shadow taps (the detail fetch is not worth its
* cost there), and fade it toward 0 with distance as a stand-in for a mip
* chain, which these volumes do not have.
*/
float CloudDensity(float3 world_position, float altitude, VolumetricCloudScapesRayConstantBuffer tuning, float total_time,
	Texture3D<float> shape_noise, Texture3D<float> detail_noise, SamplerState wrap_sampler, float detail_strength)
{
	float layer_thickness = max(tuning.cloud_top_ - tuning.cloud_bottom_, 0.0001);
	float height01 = (altitude - tuning.cloud_bottom_) / layer_thickness;
	if (height01 <= 0.0 || height01 >= 1.0)
	{
		return 0.0;
	}

	float wind_offset = total_time * tuning.wind_speed_;

	/// Weather field: a very low-frequency XZ slice of the shape volume. This
	/// is what gives the sky large-scale cloud masses and clear gaps instead of
	/// one globally uniform coverage - without it the field reads as an even
	/// carpet of identical puffs. The scale and the W slice are deliberately
	/// non-harmonic with the shape lookup below so the two do not correlate.
	float2 weather_position = world_position.xz * tuning.noise_scale_ * tuning.weather_scale_ + wind_offset * 0.15;
	float weather = shape_noise.SampleLevel(wrap_sampler, float3(weather_position, 0.713), 0);
	weather = smoothstep(0.35, 0.75, weather);

	/// Weather drives how much cloud there is AND how tall it grows, so thin
	/// flat patches and towering masses coexist in one sky. Scaling coverage
	/// (rather than offsetting it) keeps the coverage_ slider's endpoints:
	/// 0 still means no cloud anywhere.
	float coverage = saturate(tuning.coverage_ + tuning.rain_ * 0.25);
	coverage = saturate(coverage * lerp(1.0 - tuning.weather_amount_, 1.0 + tuning.weather_amount_, weather));
	if (coverage <= 0.0)
	{
		return 0.0;
	}

	float top_extent = lerp(0.35, 1.0, weather);
	float profile_height = saturate(height01 / max(top_extent, 0.0001));

	/// Height profile: stratus (flat low band) <-> cumulus (tall) by type.
	float stratus_profile = saturate((profile_height - 0.05) * 8.0) * saturate((0.4 - profile_height) * 6.0);
	float cumulus_profile = saturate(profile_height * 3.0) * saturate((1.0 - profile_height) * 1.5);
	float height_fade = lerp(stratus_profile, cumulus_profile, saturate(tuning.cloud_type_));
	if (height_fade <= 0.0)
	{
		return 0.0;
	}

	float3 shape_position = world_position * tuning.noise_scale_ + float3(wind_offset, 0.0, wind_offset * 0.3);
	float shape = shape_noise.SampleLevel(wrap_sampler, shape_position, 0);

	float base = saturate((shape - (1.0 - coverage)) / max(coverage, 0.0001));
	if (base <= 0.0)
	{
		return 0.0;
	}

	float density = base;

	if (detail_strength > 0.0)
	{
		/// The detail volume scrolls on its OWN offset. Multiplying the shape's
		/// wind offset by detail_scale_ (as the domain scale does) would drag
		/// the detail across the cloud body at detail_scale_ times the wind
		/// speed, which reads as the surface boiling rather than as texture
		/// locked to the cloud. The slight downward Y drift is what makes tops
		/// look like they are being blown off.
		float detail_wind = total_time * tuning.wind_speed_ * tuning.detail_wind_speed_;
		float3 detail_position = world_position * tuning.noise_scale_ * tuning.detail_scale_
			+ float3(detail_wind, -detail_wind * 0.5, detail_wind * 0.3);
		float detail = detail_noise.SampleLevel(wrap_sampler, detail_position, 0);

		/// Erode hardest at the edges (low base) and toward the top, which is
		/// what gives wispy tops over solid bottoms.
		float erosion_amount = tuning.detail_strength_ * detail_strength * lerp(1.0, 1.6, height01);
		float erosion = (1.0 - detail) * erosion_amount * (1.0 - base);
		density = saturate(base - erosion);
	}

	float density_boost = 1.0 + tuning.rain_ * 1.5;
	return density * height_fade * tuning.density_scale_ * density_boost;
}

// ---------------------------------------------------------------------------
// Scattering
// ---------------------------------------------------------------------------

float CloudPhaseHenyeyGreenstein(float cos_theta, float g)
{
	float g2 = g * g;
	float denominator = 1.0 + g2 - 2.0 * g * cos_theta;
	return (1.0 - g2) / (4.0 * CLOUD_PI * denominator * sqrt(max(denominator, 0.0001)));
}

/**
* Dual-lobe cloud phase: a forward lobe for the silver lining you get looking
* into the sun, plus a weaker backward lobe that keeps clouds bright with the
* sun behind you. A single Henyey-Greenstein lobe is the main reason clouds
* read as flat gauze - it can produce one of those two effects, never both.
*
* eccentricity scales both lobes toward isotropic, which is how the
* multiple-scattering octaves flatten the phase as they go.
*/
float CloudPhase(float cos_theta, VolumetricCloudScapesRayConstantBuffer tuning, float eccentricity)
{
	float forward = CloudPhaseHenyeyGreenstein(cos_theta, tuning.scattering_g_ * eccentricity);
	float backward = CloudPhaseHenyeyGreenstein(cos_theta, -tuning.phase_backward_ * eccentricity);
	return lerp(forward, backward, saturate(tuning.phase_blend_));
}

/**
* Beer-powder term. The sunlit surface of a cloud is darker than Beer's law
* predicts, because a photon needs some depth of medium before it can scatter
* back out toward the viewer. Without it, lit faces saturate into a flat white
* blob with no shape reading.
*
* light_optical_depth is the depth toward the SUN, i.e. how deep the sample sits
* below the lit surface - the quantity the effect actually depends on. Deriving
* it from the view step's extinction instead would make the powder strength a
* function of step length, so it would shift with distance as the marcher's LOD
* grows the steps.
*
* Only applied when looking roughly down-sun (sun behind the camera); looking
* into the sun the effect does not exist and would just darken the silver
* lining.
*/
float CloudPowder(float light_optical_depth, float cos_theta, float strength)
{
	float powder = 1.0 - exp(-light_optical_depth * 2.0);
	float weight = saturate(1.0 - (cos_theta * 0.5 + 0.5)) * saturate(strength);
	return lerp(1.0, powder, weight);
}

#define CLOUD_MULTI_SCATTER_MAX_OCTAVES 8

/**
* Per-ray constants for the multiple-scattering octave stack.
*
* Every octave's phase value depends only on cos_theta and the tuning, both of
* which are fixed for the whole ray - so they are hoisted here and evaluated
* ONCE per pixel instead of once per march sample. That matters: the phase is
* two Henyey-Greenstein lobes per octave, each with a sqrt and a divide, so
* leaving them in the sample loop costs 10+ of those per sample for nothing.
*
* weight_ folds the per-octave contribution into the phase. Octaves beyond
* multi_scatter_octaves_ are zeroed rather than skipped, which lets CloudSunLight
* stay a branch-free unrolled loop with constant array indices (so the arrays
* live in registers instead of spilling to scratch).
*/
struct CloudMultiScatter
{
	float weight_[CLOUD_MULTI_SCATTER_MAX_OCTAVES];
	float attenuation_[CLOUD_MULTI_SCATTER_MAX_OCTAVES];
};

CloudMultiScatter CloudMultiScatterSetup(float cos_theta, VolumetricCloudScapesRayConstantBuffer tuning)
{
	CloudMultiScatter result;

	uint octaves = min(max(tuning.multi_scatter_octaves_, 1u), (uint)CLOUD_MULTI_SCATTER_MAX_OCTAVES);

	float attenuation = 1.0;
	float contribution = 1.0;
	float eccentricity = 1.0;

	[unroll]
	for (uint octave = 0; octave < CLOUD_MULTI_SCATTER_MAX_OCTAVES; octave++)
	{
		result.weight_[octave] = octave < octaves ? contribution * CloudPhase(cos_theta, tuning, eccentricity) : 0.0;
		result.attenuation_[octave] = attenuation;

		attenuation *= tuning.multi_scatter_attenuation_;
		contribution *= tuning.multi_scatter_contribution_;
		eccentricity *= tuning.multi_scatter_eccentricity_;
	}

	return result;
}

/**
* Sun radiance reaching a sample, as a multiplier on sun_radiance.
*
* Multiple-scattering approximation (Wrenninge's octaves): the sun term is
* re-evaluated a few times with progressively weaker extinction, weaker
* contribution and a flatter phase. This is what fills in cloud interiors and
* stops a thick cloud from reading as a dark silhouette - single scattering
* alone physically cannot, no matter how the density is tuned. It costs
* arithmetic only: the optical depth is marched once and reused, and the phases
* come pre-computed from CloudMultiScatterSetup.
*/
float CloudSunLight(float light_optical_depth, float powder, CloudMultiScatter multi_scatter)
{
	float result = 0.0;

	[unroll]
	for (uint octave = 0; octave < CLOUD_MULTI_SCATTER_MAX_OCTAVES; octave++)
	{
		float transmittance = exp(-light_optical_depth * multi_scatter.attenuation_[octave]);

		/// Powder only applies to the directly-lit first order; the higher
		/// orders are diffuse enough that it would over-darken.
		float powder_term = octave == 0 ? powder : 1.0;

		result += multi_scatter.weight_[octave] * transmittance * powder_term;
	}

	return result;
}

/**
* Optical depth from a sample toward the sun.
*
* Two things matter here. First, the march length is the actual distance to
* the top of the layer along the light direction, so a low sun marches the
* long path it really travels (thickness / sin(elevation)) instead of always
* one layer thickness - that fixed length is why low-sun clouds looked flat.
* Second, the steps grow geometrically, so the near field that dominates the
* shadow is sampled densely while the tail still reaches the layer top.
*/
float CloudLightOpticalDepth(float3 position, float altitude, float3 light_direction,
	VolumetricCloudScapesRayConstantBuffer tuning, float total_time,
	Texture3D<float> shape_noise, Texture3D<float> detail_noise, SamplerState wrap_sampler,
	uint step_count, float jitter)
{
	float layer_thickness = max(tuning.cloud_top_ - tuning.cloud_bottom_, 1.0);
	float curvature = CloudCurvature(light_direction, tuning.planet_radius_);

	float march_distance = layer_thickness * 2.0;

	float exit_near;
	float exit_far;
	if (CloudSolveAltitude(altitude, light_direction.y, curvature, tuning.cloud_top_, exit_near, exit_far))
	{
		float candidate = exit_near > 0.0 ? exit_near : exit_far;
		if (candidate > 0.0)
		{
			march_distance = candidate;
		}
	}

	/// A sun on the horizon would otherwise ask for a march hundreds of layer
	/// thicknesses long.
	march_distance = clamp(march_distance, layer_thickness * 0.25, layer_thickness * 12.0);

	uint steps = max(step_count, 1u);

	/// Geometric step growth, normalised so the segments sum to march_distance.
	const float growth = 1.6;
	float weight_sum = (pow(growth, float(steps)) - 1.0) / (growth - 1.0);
	float unit = march_distance / max(weight_sum, 0.0001);

	float optical_depth = 0.0;
	float travelled = 0.0;
	float weight = 1.0;

	for (uint step = 0; step < steps; step++)
	{
		float segment = unit * weight;
		float offset = travelled + segment * (0.5 + (jitter - 0.5) * 0.8);

		float sample_altitude = CloudAltitudeAt(altitude, light_direction.y, curvature, offset);
		float3 sample_position = position + light_direction * offset;

		optical_depth += CloudDensity(sample_position, sample_altitude, tuning, total_time,
			shape_noise, detail_noise, wrap_sampler, 0.0) * segment;

		travelled += segment;
		weight *= growth;
	}

	return optical_depth;
}

// ---------------------------------------------------------------------------
// Procedural sky
// ---------------------------------------------------------------------------

/**
* Analytic procedural sky: vertical gradient + sun disc/aureole + ground fill.
* direction must be normalized; sun_direction points TOWARD the sun.
*/
float3 ProceduralSkyColor(float3 direction, float3 sun_direction, float3 sun_radiance, VolumetricCloudScapesRayConstantBuffer tuning)
{
	float up = direction.y;

	/// Above horizon: zenith->horizon gradient; below: horizon->ground.
	float horizon_blend = pow(saturate(1.0 - max(up, 0.0)), 1.8);
	float3 sky = lerp(tuning.sky_zenith_color_, tuning.sky_horizon_color_, horizon_blend);
	float3 color = lerp(sky, tuning.ground_color_, saturate(-up * 6.0));

	color *= tuning.sky_brightness_;

	if (up > -0.05)
	{
		float cos_view_sun = dot(direction, sun_direction);

		/// Sharp-edged disc with a slight smoothstep rim.
		float cos_disc = cos(tuning.sun_size_);
		float cos_rim = cos(tuning.sun_size_ * 1.6);
		float disc = smoothstep(cos_rim, cos_disc, cos_view_sun);

		/// Two-term aureole. A single tight pow() gives the sun a hard,
		/// sticker-like edge; the wide term is what sells the haze around it
		/// and what the clouds' silver lining reads against.
		float glow = pow(saturate(cos_view_sun), 96.0) * 0.06 + pow(saturate(cos_view_sun), 8.0) * 0.02;

		color += sun_radiance * (disc + glow);
	}

	return color;
}

#endif // __VOLUMETRIC_CLOUD_SCAPES_HLSL__
