struct FullscreenOutput
{
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
};

FullscreenOutput main(uint vertex_id : SV_VertexID)
{
	FullscreenOutput output;

	float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
	output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	output.texcoord = uv;

	return output;
}
