#include "../../Light/BidirectionalReflectanceDistributionFunction.hlsli"

// Charlie NDF for KHR_materials_sheen (fabric grazing-angle highlight).
// sin(theta_h) is derived as sqrt(1 - cos^2).
float CharlieDistribution(float roughness, float normal_dot_half)
{
	const float PI = 3.14159265358979;
	float alpha = max(roughness, 0.001);
	float inv_alpha = 1.0 / alpha;
	float cos2h = normal_dot_half * normal_dot_half;
	float sin2h = max(1.0 - cos2h, 0.0078125);
	return (2.0 + inv_alpha) * pow(sin2h, inv_alpha * 0.5) / (2.0 * PI);
}

// Ashikhmin visibility term for KHR_materials_sheen.
float AshikhminVisibility(float normal_dot_light, float normal_dot_view)
{
	return 1.0 / max(4.0 * (normal_dot_light + normal_dot_view - normal_dot_light * normal_dot_view), 1e-4);
}

// Physically-based direct light response: Cook-Torrance GGX specular + Lambertian
// diffuse, plus optional KHR_materials_clearcoat (a second GGX lobe with fixed
// dielectric F0, layered on top and attenuated by its own Fresnel) and
// KHR_materials_sheen (an additive Charlie lobe, not energy-conserving against
// the base layer - the full KHR spec instead scales the base by a sheen
// directional-albedo LUT).
float3 EvalDirectLight(float3 normal, float3 view, float3 light_direction, float3 diffuse_color, float3 f0, float roughness, float clearcoat, float clearcoat_roughness, float3 clearcoat_normal, float3 sheen_color, float sheen_roughness, float3 anisotropy_tangent, float3 anisotropy_bitangent, float anisotropy_strength)
{
	float normal_dot_light = max(dot(normal, light_direction), 0.0);
	if (normal_dot_light <= 0.0)
	{
		return float3(0, 0, 0);
	}

	float normal_dot_view = max(dot(normal, view), 0.001);
	float3 half_vector = normalize(view + light_direction);
	float normal_dot_half = max(dot(normal, half_vector), 0.0);
	float view_dot_half = max(dot(view, half_vector), 0.0);

	float alpha_roughness = roughness * roughness;
	float3 f90 = float3(1.0, 1.0, 1.0);

	float3 diffuse = BrdfLambertian(f0, f90, diffuse_color, view_dot_half);

	// KHR_materials_anisotropy stretches the highlight along the tangent frame.
	// Splits the single roughness into a tangent/bitangent pair; strength 0
	// leaves them equal, which is exactly the isotropic lobe, so the branch
	// only costs the materials that actually opt in.
	float3 specular;
	if (abs(anisotropy_strength) > 0.0)
	{
		float anisotropy_clamped = clamp(anisotropy_strength, -0.99, 0.99);
		float alpha_tangent = max(alpha_roughness * (1.0 + anisotropy_clamped), 0.001);
		float alpha_bitangent = max(alpha_roughness * (1.0 - anisotropy_clamped), 0.001);

		specular = BrdfSpecularGgxAnisotropic(f0, f90, alpha_tangent, alpha_bitangent, view_dot_half,
			normal_dot_light, normal_dot_view, normal_dot_half,
			dot(anisotropy_tangent, light_direction), dot(anisotropy_bitangent, light_direction),
			dot(anisotropy_tangent, view), dot(anisotropy_bitangent, view),
			dot(anisotropy_tangent, half_vector), dot(anisotropy_bitangent, half_vector));
	}
	else
	{
		specular = BrdfSpecularGgx(f0, f90, alpha_roughness, view_dot_half, normal_dot_light, normal_dot_view, normal_dot_half);
	}

	float3 base = diffuse + specular;

	// Clearcoat: a second GGX lobe with a fixed dielectric F0 (IOR ~1.5 -> 0.04)
	// layered on top; the base layer is attenuated by the coat Fresnel.
	if (clearcoat > 0.0)
	{
		// The coat lobe uses its own normal (KHR_materials_clearcoat's
		// clearcoatNormalTexture) - it is a separate layer and may be smooth
		// where the base is bumpy. Equals the base normal when the material
		// has no coat normal map.
		float coat_normal_dot_light = max(dot(clearcoat_normal, light_direction), 0.0);
		float coat_normal_dot_view = max(dot(clearcoat_normal, view), 0.001);
		float coat_normal_dot_half = max(dot(clearcoat_normal, half_vector), 0.0);

		const float3 coat_f0 = float3(0.04, 0.04, 0.04);
		float coat_alpha = clearcoat_roughness * clearcoat_roughness;
		float3 coat_fresnel = FresnelSchlick(coat_f0, f90, view_dot_half) * clearcoat;
		float3 coat_specular = BrdfSpecularGgx(coat_f0, f90, coat_alpha, view_dot_half, coat_normal_dot_light, coat_normal_dot_view, coat_normal_dot_half) * clearcoat;
		base = base * (1.0 - coat_fresnel) + coat_specular;
	}

	if (dot(sheen_color, sheen_color) > 0.0)
	{
		float sheen_ndf = CharlieDistribution(sheen_roughness, normal_dot_half);
		float sheen_visibility = AshikhminVisibility(normal_dot_light, normal_dot_view);
		base += sheen_color * sheen_ndf * sheen_visibility;
	}

	return base * normal_dot_light;
}
