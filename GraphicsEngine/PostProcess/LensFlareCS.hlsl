#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Six-armed diffraction-spike ("starburst") lens flare. Runs at a quarter of
* the native resolution, writing an additive RGB contribution that
* ToneMappingCS.hlsl samples and adds into the HDR color before exposure.
*
* For every output pixel, gathers bright-pass samples of the same HDR source
* ToneMappingCS.hlsl reads along STAR_ARM_COUNT fixed directions spaced
* 2*pi/STAR_ARM_COUNT apart - with 6 arms that is 60 degrees, so opposing
* arms fall on the same axis and each bright point ends up with a symmetric
* spike through it rather than six one-sided rays. Sample weight decays
* exponentially with distance (streak_attenuation_) out to streak_length_.
* Each channel is sampled at a slightly different distance along the arm
* (chromatic_aberration_) for the color fringing real diffraction spikes
* show.
*
* ---------------------------------------------------------------------
*
* [JP]
* 6本腕の回折スパイク(スターバースト)型レンズフレア。ネイティブ解像度の
* 1/4で走り、加算合成用のRGB寄与を書き込む — ToneMappingCS.hlsl がそれを
* サンプルして露出適用前のHDRカラーへ加算する。
*
* 出力ピクセルごとに、ToneMappingCS.hlsl が読むのと同じHDRソースの
* ブライトパスを、2*pi/STAR_ARM_COUNT 間隔(6本なら60度間隔)の固定方向に
* 沿ってゲザーする — 対になる腕が同じ軸に乗るので、明るい点ごとに片側だけの
* 光線6本ではなく対称なスパイクになる。サンプル重みは距離
* (streak_attenuation_)に応じて指数的に減衰し、streak_length_ まで届く。
* 実物の回折スパイクが見せる色収差を再現するため、同じ腕上でチャンネルごとに
* 少しずつ異なる距離(chromatic_aberration_)でサンプルする。
*/

#define STAR_ARM_COUNT 6
#define STAR_STEP_COUNT 10

float3 BrightPass(float3 color, float threshold)
{
	float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
	float weight = max(luminance - threshold, 0.0) / max(luminance, 0.0001);
	return color * weight;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> output = ResourceDescriptorHeap[constant_indices.post_process_.lens_flare_.unordered_access_view_index_];

	uint width, height;
	output.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	// [JP] 被写界深度が有効ならDepthOfFieldCS.hlsl(+BokehCS.hlsl)が書いた
	//      ぼかし済みバッファを、無効なら生のHDRソースを読む。
	uint source_index = constant_indices.post_process_.depth_of_field_.enabled_ != 0 ? constant_indices.post_process_.depth_of_field_.shader_resource_view_index_ : constant_indices.post_process_.source_color_index_;
	Texture2D<float4> source = ResourceDescriptorHeap[source_index];

	float threshold = constant_indices.post_process_.lens_flare_.threshold_;
	float streak_length = constant_indices.post_process_.lens_flare_.streak_length_;
	float streak_attenuation = constant_indices.post_process_.lens_flare_.streak_attenuation_;
	float chromatic_aberration = constant_indices.post_process_.lens_flare_.chromatic_aberration_;
	float angle_offset = constant_indices.post_process_.lens_flare_.angle_offset_;

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float step_length = streak_length / float(STAR_STEP_COUNT);

	float3 accum = float3(0.0, 0.0, 0.0);

	for (uint arm = 0; arm < STAR_ARM_COUNT; arm++)
	{
		float angle = angle_offset + (6.283185307 / float(STAR_ARM_COUNT)) * float(arm);
		float2 direction = float2(cos(angle), sin(angle));

		for (uint step = 1; step <= STAR_STEP_COUNT; step++)
		{
			float t = step_length * float(step);
			float weight = exp(-streak_attenuation * (float(step) / float(STAR_STEP_COUNT)));
			float2 fringe_offset = direction * chromatic_aberration * float(step);

			float r = source.SampleLevel(sampler_linear_clamp, uv + direction * t + fringe_offset, 0).r;
			float g = source.SampleLevel(sampler_linear_clamp, uv + direction * t, 0).g;
			float b = source.SampleLevel(sampler_linear_clamp, uv + direction * t - fringe_offset, 0).b;

			accum += BrightPass(float3(r, g, b), threshold) * weight;
		}
	}

	output[dtid.xy] = float4(accum * constant_indices.post_process_.lens_flare_.intensity_, 1.0);
}
