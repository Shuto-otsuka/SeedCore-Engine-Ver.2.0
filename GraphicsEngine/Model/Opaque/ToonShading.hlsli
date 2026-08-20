// Toon (cel) shading: N.L diffuse quantized into discrete bands, plus a
// specular highlight cut to a hard edge by a threshold instead of a smooth
// falloff.
float3 EvalDirectLightToon(float3 normal, float3 view, float3 light_direction, float3 diffuse_color, float3 specular_color, float shininess)
{
	const int TOON_BANDS = 3;

	float normal_dot_light = max(dot(normal, light_direction), 0.0);
	float band = floor(normal_dot_light * TOON_BANDS) / max(TOON_BANDS - 1, 1);
	float3 diffuse = diffuse_color * band;

	float3 half_vector = normalize(view + light_direction);
	float normal_dot_half = max(dot(normal, half_vector), 0.0);
	float specular_term = pow(normal_dot_half, shininess);
	float3 specular = specular_color * step(0.5, specular_term) * step(0.0, normal_dot_light);

	return diffuse + specular;
}
