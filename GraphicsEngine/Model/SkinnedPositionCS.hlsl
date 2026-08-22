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

	float3 skinned = mul(float4(position, 1.0), skin_matrix).xyz;

	/// [EN] These positions become a BLAS's triangle vertices. A non-finite
	///      vertex builds a degenerate acceleration structure, and DXR
	///      traversal over one never terminates - the GPU hangs and the
	///      device is lost with no page fault to point at. Fall back to the
	///      unskinned position so the frame renders wrong rather than dying.
	/// [JP] ここで書いた位置はそのまま BLAS の三角形頂点になる。非有限な頂点は
	///      退化した加速構造を作り、その上の DXR 走査は終了しない - GPU が
	///      ハングし、手がかりとなるページフォルトも無いままデバイスが失われる。
	///      スキン適用前の位置へフォールバックし、死ぬ代わりに絵が崩れるだけに
	///      留める。
	if (!all(isfinite(skinned)))
	{
		skinned = position;
	}

	skinned_positions[index] = skinned;
}
