#include "../Shader/Denoiser.hlsli"

static const float TAAU_TEMPORAL_BLEND_ALPHA = 0.1;
static const float TAAU_HISTORY_CLIP_GAMMA = 0.75;

struct TaauResolveConstants
{
	uint color_index_;
	uint depth_index_;
	uint velocity_index_;
	uint history_index_;

	uint destination_index_;
	uint source_width_;
	uint source_height_;
	uint destination_width_;

	uint destination_height_;
	uint taau_resolve_padding_0_;
	uint taau_resolve_padding_1_;
	uint taau_resolve_padding_2_;
};
ConstantBuffer<TaauResolveConstants> taau_resolve : register(b0, space1);

float4 CatmullRomWeights(float t)
{
	float t2 = t * t;
	float t3 = t2 * t;

	float4 weights;
	weights.x = -0.5 * t3 + 1.0 * t2 - 0.5 * t;
	weights.y = 1.5 * t3 - 2.5 * t2 + 1.0;
	weights.z = -1.5 * t3 + 2.0 * t2 + 0.5 * t;
	weights.w = 0.5 * t3 - 0.5 * t2;
	return weights;
}

float3 SampleCatmullRom(Texture2D<float4> source_texture, float2 source_pixel_f, int2 source_max)
{
	int2 base_pixel = int2(floor(source_pixel_f - 0.5));
	float2 fraction = (source_pixel_f - 0.5) - float2(base_pixel);

	float4 weights_x = CatmullRomWeights(fraction.x);
	float4 weights_y = CatmullRomWeights(fraction.y);

	float3 result = float3(0, 0, 0);

	[unroll]
	for (int row = 0; row < 4; ++row)
	{
		float3 row_sum = float3(0, 0, 0);

		[unroll]
		for (int col = 0; col < 4; ++col)
		{
			int2 tap = clamp(base_pixel + int2(col - 1, row - 1), int2(0, 0), source_max);
			row_sum += source_texture.Load(int3(tap, 0)).rgb * weights_x[col];
		}

		result += row_sum * weights_y[row];
	}

	return result;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	if (dtid.x >= taau_resolve.destination_width_ || dtid.y >= taau_resolve.destination_height_)
	{
		return;
	}

	uint2 destination_pixel = dtid.xy;
	int2 source_max = int2(taau_resolve.source_width_ - 1, taau_resolve.source_height_ - 1);

	float2 destination_size = float2(taau_resolve.destination_width_, taau_resolve.destination_height_);
	float2 source_size = float2(taau_resolve.source_width_, taau_resolve.source_height_);

	float2 output_uv = (float2(destination_pixel) + 0.5) / destination_size;
	float2 source_pixel_f = output_uv * source_size;
	int2 center_pixel = clamp(int2(round(source_pixel_f - 0.5)), int2(0, 0), source_max);

	Texture2D<float4> color_texture = ResourceDescriptorHeap[taau_resolve.color_index_];
	float3 filtered_raw = SampleCatmullRom(color_texture, source_pixel_f, source_max);

	DenoiserMoments moments = DenoiserMomentsInit();

	[unroll]
	for (int dy = -1; dy <= 1; ++dy)
	{
		[unroll]
		for (int dx = -1; dx <= 1; ++dx)
		{
			int2 neighbor = clamp(center_pixel + int2(dx, dy), int2(0, 0), source_max);
			float3 neighbor_color = color_texture.Load(int3(neighbor, 0)).rgb;
			DenoiserMomentsAccumulate(moments, neighbor_color, 1.0);
		}
	}

	float3 clip_min = DenoiserVarianceClipMin(moments, filtered_raw, TAAU_HISTORY_CLIP_GAMMA);
	float3 clip_max = DenoiserVarianceClipMax(moments, filtered_raw, TAAU_HISTORY_CLIP_GAMMA);

	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[taau_resolve.velocity_index_];
	float2 velocity = velocity_texture.Load(int3(center_pixel, 0));
	float2 delta_uv = float2(velocity.x, -velocity.y);
	float2 previous_output_uv = output_uv - delta_uv;

	Texture2D<float4> history_texture = ResourceDescriptorHeap[taau_resolve.history_index_];
	float3 result = DenoiserTemporalBlend(history_texture, previous_output_uv, clip_min, clip_max, filtered_raw, TAAU_TEMPORAL_BLEND_ALPHA);

	RWTexture2D<float4> destination = ResourceDescriptorHeap[taau_resolve.destination_index_];
	destination[destination_pixel] = float4(result, 1.0);
}
