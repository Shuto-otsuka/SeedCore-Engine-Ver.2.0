struct PSInput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

cbuffer SplashConstantBuffer : register(b0)
{
	float alpha_;
	float screen_width_;
	float screen_height_;
	float texture_aspect_;
	float show_logo_;
};

Texture2D splash_texture : register(t0);
SamplerState splash_sampler : register(s0);

float4 main(PSInput input) : SV_Target
{
	float screen_aspect = screen_width_ / screen_height_;

	float scale = 1.0f;

	float2 center = float2(0.5f, 0.5f);
	float2 half_size;

	if (texture_aspect_ > screen_aspect)
	{
		half_size.x = scale * 0.5f;
		half_size.y = half_size.x * screen_aspect / texture_aspect_;
	}
	else
	{
		half_size.y = scale * 0.5f;
		half_size.x = half_size.y * texture_aspect_ / screen_aspect;
	}

	float2 min_uv = center - half_size;
	float2 max_uv = center + half_size;

	if (show_logo_ > 0.5f && input.uv.x >= min_uv.x && input.uv.x <= max_uv.x && input.uv.y >= min_uv.y && input.uv.y <= max_uv.y)
	{
		float2 tex_coord = (input.uv - min_uv) / (max_uv - min_uv);
		float4 tex_color = splash_texture.Sample(splash_sampler, tex_coord);
		return float4(tex_color.rgb, tex_color.a * alpha_);
	}

	return float4(0.0f, 0.0f, 0.0f, alpha_);
}
