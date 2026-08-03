#include "SkyMath.hlsli"
#include "SkyGenerate.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Convolves the environment cube into a diffuse-irradiance cube by integrating
* the cosine-weighted hemisphere around each output direction. One thread per
* irradiance cube texel.
*
* [JP]
* environment キューブを拡散 irradiance キューブへ畳み込む。各出力方向まわりの
* コサイン重み半球を積分する。irradiance キューブテクセル 1 つにつき 1 スレッド。
*/
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint size = sky_generate.face_size_;
	if (id.x >= size || id.y >= size)
	{
		return;
	}

	uint face = id.z;
	float2 uv = (float2(id.xy) + 0.5) / float(size) * 2.0 - 1.0;
	float3 normal = CubeFaceDirection(face, uv);

	TextureCube<float4> environment = ResourceDescriptorHeap[sky_generate.source_index_];
	RWTexture2DArray<float4> destination = ResourceDescriptorHeap[sky_generate.dest_index_];

	float3 up = abs(normal.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
	float3 right = normalize(cross(up, normal));
	up = cross(normal, right);

	float3 irradiance = float3(0.0, 0.0, 0.0);
	float sample_count = 0.0;

	const float delta_phi = 0.025;
	const float delta_theta = 0.025;

	for (float phi = 0.0; phi < 2.0 * SKY_PI; phi += delta_phi)
	{
		for (float theta = 0.0; theta < 0.5 * SKY_PI; theta += delta_theta)
		{
			float3 tangent_sample = cos(phi) * sin(theta) * right + sin(phi) * sin(theta) * up + cos(theta) * normal;
			irradiance += environment.SampleLevel(sampler_linear_clamp, tangent_sample, 0).rgb * cos(theta) * sin(theta);
			sample_count += 1.0;
		}
	}

	irradiance = SKY_PI * irradiance / max(sample_count, 1.0);

	destination[uint3(id.xy, face)] = float4(irradiance, 1.0);
}
