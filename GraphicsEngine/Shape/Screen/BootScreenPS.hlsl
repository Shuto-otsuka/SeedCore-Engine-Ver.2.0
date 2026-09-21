#define PI 3.14159265f

struct PSInput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

struct BootScreenConstantBuffer
{
	float2 screen_size_;
	float alpha_;
	float progress_;

	float time_;
	float background_aspect_;
	uint fill_method_;
	uint fill_origin_;

	uint clockwise_;
	uint wave_;
	uint use_background_image_;
	uint use_frame_;

	float4 bar_rect_;

	float4 background_color_;
	float4 background_tint_;
	float4 bar_tint_;
	float4 frame_tint_;
};

ConstantBuffer<BootScreenConstantBuffer> boot_screen_constant_buffer : register(b0);

Texture2D background_texture : register(t0);
Texture2D bar_texture : register(t1);
Texture2D frame_texture : register(t2);
SamplerState boot_screen_sampler : register(s0);

static const float2 radial90_pivots[4] = { float2(0.0f, 1.0f), float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(1.0f, 1.0f) };
static const float2 radial90_middles[4] = { float2(1.0f, -1.0f), float2(1.0f, 1.0f), float2(-1.0f, 1.0f), float2(-1.0f, -1.0f) };
static const float2 radial180_pivots[4] = { float2(0.5f, 1.0f), float2(0.0f, 0.5f), float2(0.5f, 0.0f), float2(1.0f, 0.5f) };
static const float2 radial180_middles[4] = { float2(0.0f, -1.0f), float2(1.0f, 0.0f), float2(0.0f, 1.0f), float2(-1.0f, 0.0f) };
static const float2 radial360_middles[4] = { float2(0.0f, -1.0f), float2(-1.0f, 0.0f), float2(0.0f, 1.0f), float2(1.0f, 0.0f) };

float LinearFillMask(float fill_coordinate, float cross_coordinate, float progress, uint wave, float time)
{
	float wave_offset = 0.0f;
	if (wave != 0)
	{
		wave_offset = sin(cross_coordinate * 25.0f + time * 3.0f) * 0.025f;
		wave_offset += sin(cross_coordinate * 12.0f - time * 1.8f + 1.7f) * 0.0125f;
		wave_offset *= saturate(progress * 20.0f) * saturate((1.0f - progress) * 20.0f);
	}

	return step(fill_coordinate, saturate(progress + wave_offset));
}

float RadialFillMask(float2 bar_uv, float2 pivot, float2 middle, float sweep, float progress, uint clockwise)
{
	float2 direction = bar_uv - pivot;
	float angle = atan2(direction.y, direction.x);
	float middle_angle = atan2(middle.y, middle.x);

	float relative_angle = (middle_angle + sweep * 0.5f) - angle;
	if (clockwise != 0)
	{
		relative_angle = angle - (middle_angle - sweep * 0.5f);
	}
	relative_angle -= floor(relative_angle / (2.0f * PI)) * (2.0f * PI);

	return step(relative_angle, progress * sweep);
}

float FillMask(float2 bar_uv, BootScreenConstantBuffer constants)
{
	float progress = saturate(constants.progress_);
	uint origin = min(constants.fill_origin_, 3u);

	if (constants.fill_method_ == 0)
	{
		float fill_coordinate = bar_uv.x;
		if (origin != 0)
		{
			fill_coordinate = 1.0f - bar_uv.x;
		}
		return LinearFillMask(fill_coordinate, bar_uv.y, progress, constants.wave_, constants.time_);
	}

	if (constants.fill_method_ == 1)
	{
		float fill_coordinate = 1.0f - bar_uv.y;
		if (origin != 0)
		{
			fill_coordinate = bar_uv.y;
		}
		return LinearFillMask(fill_coordinate, bar_uv.x, progress, constants.wave_, constants.time_);
	}

	if (constants.fill_method_ == 2)
	{
		return RadialFillMask(bar_uv, radial90_pivots[origin], radial90_middles[origin], PI * 0.5f, progress, constants.clockwise_);
	}

	if (constants.fill_method_ == 3)
	{
		return RadialFillMask(bar_uv, radial180_pivots[origin], radial180_middles[origin], PI, progress, constants.clockwise_);
	}

	return RadialFillMask(bar_uv, float2(0.5f, 0.5f), radial360_middles[origin], 2.0f * PI, progress, constants.clockwise_);
}

float4 main(PSInput input) : SV_Target
{
	BootScreenConstantBuffer constants = boot_screen_constant_buffer;

	float screen_aspect = constants.screen_size_.x / constants.screen_size_.y;

	float3 color = constants.background_color_.rgb;

	if (constants.use_background_image_ != 0)
	{
		float2 background_half_size;

		if (constants.background_aspect_ > screen_aspect)
		{
			background_half_size.y = 0.5f;
			background_half_size.x = background_half_size.y * constants.background_aspect_ / screen_aspect;
		}
		else
		{
			background_half_size.x = 0.5f;
			background_half_size.y = background_half_size.x * screen_aspect / constants.background_aspect_;
		}

		float2 background_min_uv = float2(0.5f, 0.5f) - background_half_size;
		float2 background_max_uv = float2(0.5f, 0.5f) + background_half_size;
		float2 background_tex_coord = (input.uv - background_min_uv) / (background_max_uv - background_min_uv);

		float4 background = background_texture.Sample(boot_screen_sampler, background_tex_coord) * constants.background_tint_;
		color = lerp(color, background.rgb, background.a);
	}

	float2 bar_min = constants.bar_rect_.xy;
	float2 bar_max = constants.bar_rect_.zw;

	if (all(input.uv >= bar_min) && all(input.uv <= bar_max))
	{
		float2 bar_uv = (input.uv - bar_min) / (bar_max - bar_min);

		float4 bar_color = bar_texture.Sample(boot_screen_sampler, bar_uv) * constants.bar_tint_;
		bar_color.a *= FillMask(bar_uv, constants);
		color = lerp(color, bar_color.rgb, bar_color.a);

		if (constants.use_frame_ != 0)
		{
			float4 frame_color = frame_texture.Sample(boot_screen_sampler, bar_uv) * constants.frame_tint_;
			color = lerp(color, frame_color.rgb, frame_color.a);
		}
	}

	return float4(color, constants.alpha_);
}
