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
	float progress_;
	float show_progress_;
	float bar_aspect_;
	float background_aspect_;
	float time_;
};

Texture2D splash_texture : register(t0);
Texture2D progress_bar_texture : register(t1);
Texture2D progress_frame_texture : register(t2);
Texture2D progress_background_texture : register(t3);
SamplerState splash_sampler : register(s0);

float4 main(PSInput input) : SV_Target
{
	float screen_aspect = screen_width_ / screen_height_;

	if (show_logo_ > 0.5f)
	{
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

		if (input.uv.x >= min_uv.x && input.uv.x <= max_uv.x && input.uv.y >= min_uv.y && input.uv.y <= max_uv.y)
		{
			float2 tex_coord = (input.uv - min_uv) / (max_uv - min_uv);
			float4 tex_color = splash_texture.Sample(splash_sampler, tex_coord);
			return float4(tex_color.rgb, tex_color.a * alpha_);
		}
	}
	else if (show_progress_ > 0.5f)
	{
		/// [EN] Background fills the whole screen (crop-to-fill, so no letterbox
		///      bars around it - the overflowing side is simply cropped).
		/// [JP] 背景は画面全体を覆う（クロップして敷き詰めるのでレターボックスの
		///      余白は出ない - はみ出した側は単純にクロップされる）。
		float2 background_half_size;

		if (background_aspect_ > screen_aspect)
		{
			background_half_size.y = 0.5f;
			background_half_size.x = background_half_size.y * background_aspect_ / screen_aspect;
		}
		else
		{
			background_half_size.x = 0.5f;
			background_half_size.y = background_half_size.x * screen_aspect / background_aspect_;
		}

		float2 background_min_uv = float2(0.5f, 0.5f) - background_half_size;
		float2 background_max_uv = float2(0.5f, 0.5f) + background_half_size;
		float2 background_tex_coord = (input.uv - background_min_uv) / (background_max_uv - background_min_uv);

		float3 color = progress_background_texture.Sample(splash_sampler, background_tex_coord).rgb;

		float bar_scale = 0.4f;

		float2 bar_half_size;

		if (bar_aspect_ > screen_aspect)
		{
			bar_half_size.x = bar_scale * 0.5f;
			bar_half_size.y = bar_half_size.x * screen_aspect / bar_aspect_;
		}
		else
		{
			bar_half_size.y = bar_scale * 0.5f;
			bar_half_size.x = bar_half_size.y * bar_aspect_ / screen_aspect;
		}

		/// [EN] Anchored to the bottom-right corner instead of bottom-center -
		///      margin_y is a fraction of screen height, margin_x is converted
		///      from the same fraction so the gap looks equal in both directions
		///      in pixels regardless of aspect ratio.
		/// [JP] 中央下ではなく右下隅を基準に配置する - margin_y は画面高さに
		///      対する割合、margin_x はアスペクト比によらずピクセル換算で
		///      同じ余白に見えるよう、その割合から変換したもの。
		float margin_y = 0.05f;
		float margin_x = margin_y / screen_aspect;

		float2 bar_center = float2(1.0f - margin_x - bar_half_size.x, 1.0f - margin_y - bar_half_size.y);

		float2 bar_min_uv = bar_center - bar_half_size;
		float2 bar_max_uv = bar_center + bar_half_size;

		if (input.uv.x >= bar_min_uv.x && input.uv.x <= bar_max_uv.x && input.uv.y >= bar_min_uv.y && input.uv.y <= bar_max_uv.y)
		{
			float2 bar_tex_coord = (input.uv - bar_min_uv) / (bar_max_uv - bar_min_uv);

			float4 bar_color = progress_bar_texture.Sample(splash_sampler, bar_tex_coord);
			float4 frame_color = progress_frame_texture.Sample(splash_sampler, bar_tex_coord);

			/// [EN] Two overlaid sine waves (different frequency/speed/phase) bend
			///      the fill's leading edge over time instead of a flat vertical
			///      cut, so it undulates like water lapping at the fill line.
			/// [JP] 2つの正弦波（周波数/速度/位相違い）を重ねて、塗りつぶしの
			///      先端を平らな垂直カットではなく時間とともに波打たせる。
			///      水面が塗りつぶし境界で揺らめくような見た目になる。
			float wave = sin(bar_tex_coord.y * 25.0f + time_ * 3.0f) * 0.025f;
			wave += sin(bar_tex_coord.y * 12.0f - time_ * 1.8f + 1.7f) * 0.0125f;

			bar_color.a *= step(bar_tex_coord.x, saturate(progress_ + wave));

			color = lerp(color, bar_color.rgb, bar_color.a);
			color = lerp(color, frame_color.rgb, frame_color.a);
		}

		return float4(color, alpha_);
	}

	return float4(0.0f, 0.0f, 0.0f, alpha_);
}
