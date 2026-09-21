struct PSInput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

Texture2D letter_texture : register(t0);
SamplerState letter_sampler : register(s0);

float4 main(PSInput input) : SV_Target
{
	return float4(letter_texture.Sample(letter_sampler, input.uv).rgb, 1.0f);
}
