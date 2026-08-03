#include "Model.hlsli"

struct SkinnedPositionParams
{
	uint vertex_count_;
	uint bone_offset_;
	uint pad0_;
	uint pad1_;
};

ConstantBuffer<SkinnedPositionParams> params : register(b0);
StructuredBuffer<float3> rt_positions : register(t0);
StructuredBuffer<ModelSkinVertex> rt_skin_vertices : register(t1);
StructuredBuffer<ModelBoneMatrix> bone_matrices : register(t2);
RWStructuredBuffer<float3> skinned_positions : register(u0);

[NumThreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint index = id.x;
	if (index >= params.vertex_count_)
	{
		return;
	}

	uint4 joints;
	float4 weights;
	DecodeModelSkinVertex(rt_skin_vertices[index], joints, weights);

	float3 position = rt_positions[index];

	if (dot(weights, 1.0) < 1e-5)
	{
		skinned_positions[index] = position;
		return;
	}

	float4x4 skin_matrix =
		LoadBoneMatrix(bone_matrices[params.bone_offset_ + joints.x]) * weights.x +
		LoadBoneMatrix(bone_matrices[params.bone_offset_ + joints.y]) * weights.y +
		LoadBoneMatrix(bone_matrices[params.bone_offset_ + joints.z]) * weights.z +
		LoadBoneMatrix(bone_matrices[params.bone_offset_ + joints.w]) * weights.w;

	skinned_positions[index] = mul(float4(position, 1.0), skin_matrix).xyz;
}
