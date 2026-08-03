#include "SkyMath.hlsli"
#include "SkyGenerate.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Prefilters the environment cube for a single roughness level (one mip of the
* prefiltered specular cube) using GGX importance sampling. One thread per
* destination cube texel. At roughness 0 it copies the environment directly.
*
* [JP]
* GGX 重点サンプリングで environment キューブを 1 つの roughness レベル
* （プリフィルタ鏡面キューブの 1 ミップ）へプリフィルタする。出力キューブ
* テクセル 1 つにつき 1 スレッド。roughness 0 では environment をそのままコピー。
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
	float3 view = normal;

	TextureCube<float4> environment = ResourceDescriptorHeap[sky_generate.source_index_];
	RWTexture2DArray<float4> destination = ResourceDescriptorHeap[sky_generate.dest_index_];

	float roughness = sky_generate.roughness_;

	if (roughness < 0.001)
	{
		float3 color = environment.SampleLevel(sampler_linear_clamp, normal, 0).rgb;
		destination[uint3(id.xy, face)] = float4(color, 1.0);
		return;
	}

	uint count = max(sky_generate.sample_count_, 1u);
	float3 prefiltered = float3(0.0, 0.0, 0.0);
	float total_weight = 0.0;

	for (uint i = 0; i < count; i++)
	{
		float2 xi = Hammersley(i, count);
		float3 half_vector = ImportanceSampleGgx(xi, normal, roughness);
		float3 light = normalize(2.0 * dot(view, half_vector) * half_vector - view);

		float normal_dot_light = dot(normal, light);
		if (normal_dot_light > 0.0)
		{
			prefiltered += environment.SampleLevel(sampler_linear_clamp, light, 0).rgb * normal_dot_light;
			total_weight += normal_dot_light;
		}
	}

	prefiltered /= max(total_weight, 0.001);

	destination[uint3(id.xy, face)] = float4(prefiltered, 1.0);
}
