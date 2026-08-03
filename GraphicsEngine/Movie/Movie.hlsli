struct MovieASPayload
{
	uint instance_indices[32];
};

struct MovieMSOutput
{
	float4 position : SV_Position;
	float2 uv        : TEXCOORD0;
	float4 color     : COLOR0;
	nointerpolation uint texture_index : BLENDINDICES;
};

struct MovieFullscreenMSOutput
{
	float4 position : SV_Position;
	float2 uv        : TEXCOORD0;
	float4 color     : COLOR0;
	nointerpolation uint texture_index    : BLENDINDICES;
	nointerpolation float texture_aspect  : TEXCOORD1;
};
