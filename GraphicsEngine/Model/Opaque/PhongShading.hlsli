// Phong: Lambertian N.L diffuse + energy-normalized R.V specular lobe
// ((n+2)/(2*pi), the modified Phong BRDF). Skips the GGX/Fresnel/microfacet
// terms entirely - roughness is only mapped down to a shininess exponent.
float3 EvalDirectLightPhong(float3 normal, float3 view, float3 light_direction, float3 diffuse_color, float3 specular_color, float shininess)
{
	const float PI = 3.14159265358979;

	float normal_dot_light = max(dot(normal, light_direction), 0.0);
	if (normal_dot_light <= 0.0)
	{
		return float3(0, 0, 0);
	}

	float3 reflection_direction = reflect(-light_direction, normal);
	float reflection_dot_view = max(dot(reflection_direction, view), 0.0);

	float3 diffuse = diffuse_color / PI;
	float3 specular = specular_color * ((shininess + 2.0) / (2.0 * PI)) * pow(reflection_dot_view, shininess);

	return (diffuse + specular) * normal_dot_light;
}
