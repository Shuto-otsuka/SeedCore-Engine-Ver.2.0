struct PSInput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

cbuffer FadeConstantBuffer : register(b0)
{
	float alpha;
};

float4 main(PSInput input) : SV_Target
{
	return float4(0.0f, 0.0f, 0.0f, alpha);
}
