// Blinn-Phong: classic N.L diffuse + N.H specular highlight. Skips the
// GGX/Fresnel/microfacet terms entirely - roughness is only mapped down to a
// shininess exponent as a coarse approximation.
float3 EvalDirectLightPhong(float3 normal, float3 view, float3 light_direction, float3 diffuse_color, float3 specular_color, float shininess)
{
	float normal_dot_light = max(dot(normal, light_direction), 0.0);
	if (normal_dot_light <= 0.0)
	{
		return float3(0, 0, 0);
	}

	float3 half_vector = normalize(view + light_direction);
	float normal_dot_half = max(dot(normal, half_vector), 0.0);

	float3 diffuse = diffuse_color;
	float3 specular = specular_color * pow(normal_dot_half, shininess);

	return (diffuse + specular) * normal_dot_light;
}
