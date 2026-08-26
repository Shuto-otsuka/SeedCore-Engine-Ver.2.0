struct HUDComposeOutput
{
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
};

[NumThreads(3, 1, 1)]
[OutputTopology("triangle")]
void main(uint gtid : SV_GroupThreadID, out vertices HUDComposeOutput verts[3], out indices uint3 tris[1])
{
	SetMeshOutputCounts(3, 1);

	float2 uv = float2((gtid << 1) & 2, gtid & 2);
	verts[gtid].position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	verts[gtid].texcoord = uv;

	if (gtid == 0)
	{
		tris[0] = uint3(0, 1, 2);
	}
}
