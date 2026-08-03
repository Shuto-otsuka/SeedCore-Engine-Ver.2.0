struct ImageASPayload
{
	uint image_indices[32];
};

struct ImageMSOutput
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
	float4 color    : COLOR0;
	nointerpolation uint texture_index : BLENDINDICES;
};
