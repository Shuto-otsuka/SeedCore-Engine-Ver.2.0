#include "../Model.hlsli"

/**
* [EN]
* Fullscreen Mesh Shader for OIT resolve pass.
* Emits a single triangle covering the entire screen using
* 3 threads / 3 vertices / 1 triangle.
*
* ---------------------------------------------------------------------
*
* [JP]
* OIT リゾルブパス用のフルスクリーン Mesh Shader。
* 3 スレッド / 3 頂点 / 1 三角形でスクリーン全体を覆う
* 単一の三角形を出力する。
*/
[NumThreads(3, 1, 1)]
[OutputTopology("triangle")]
void main(uint gtid : SV_GroupThreadID, out vertices OITResolveOutput verts[3], out indices uint3 tris[1])
{
	SetMeshOutputCounts(3, 1);

	float2 uv = float2((gtid << 1) & 2, gtid & 2);
	verts[gtid].position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

	if (gtid == 0)
	{
		tris[0] = uint3(0, 1, 2);
	}
}
