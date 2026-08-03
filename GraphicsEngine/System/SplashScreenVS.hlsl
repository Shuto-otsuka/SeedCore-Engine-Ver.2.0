struct VSOutput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

VSOutput main(uint vertex_id : SV_VertexID)
{
	VSOutput output;

	float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
	output.position = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
	output.position.y = -output.position.y;
	output.uv = uv;

	return output;
}
