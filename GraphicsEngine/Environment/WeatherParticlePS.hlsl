#include "WeatherParticle.hlsli"

// Additive blend (SrcBlend=SRC_ALPHA, DestBlend=ONE - see
// WeatherParticleShader.cpp), so alpha must be output as plain coverage, NOT
// pre-multiplied into color_ (the blend unit already does color*alpha).

float4 main(WeatherParticleMSOutput input) : SV_Target0
{
	float alpha;

	if (input.isRain_ != 0)
	{
		// Thin streak: tight falloff across the width, soft rounded ends
		// along the length.
		float widthFalloff = exp(-input.local_.x * input.local_.x * 12.0);
		float lengthFalloff = saturate(1.0 - abs(input.local_.y));
		alpha = widthFalloff * lengthFalloff;
	}
	else
	{
		// Soft round flake.
		float distanceSquared = dot(input.local_, input.local_);
		alpha = saturate(1.0 - distanceSquared);
		alpha = alpha * alpha;
	}

	if (alpha <= 0.001)
	{
		discard;
	}

	return float4(input.color_, alpha);
}
