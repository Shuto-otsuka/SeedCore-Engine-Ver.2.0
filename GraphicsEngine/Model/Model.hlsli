#include "../Shader/Normal.hlsli"

// Decoded vertex, produced by DecodeModelVertex. Skinning attributes live in
// the separate ModelSkinVertex buffer (skinned models only).
struct ModelVertex
{
	float3 position_;
	float3 normal_;
	float4 tangent_;
	float2 texcoord_;
};

// Mirrors the C++ CompressedVertex (Model/Crister.h) - 16 bytes. Quantised on
// the CPU at upload; the encode there must match DecodeModelVertex exactly.
struct CompressedModelVertex
{
	uint position_xy_;    // x:16 | y:16 (UNORM in the instance position AABB)
	uint position_z_texu_;// z:16 | texcoord u:16 (UNORM in the instance UV AABB)
	uint texv_tangent_;   // texcoord v:16 | tangent oct x:8 y:7 sign:1
	uint normal_;         // octahedral x:16 | y:16
};

// Mirrors the C++ CompressedSkinVertex (Model/Crister.h) - 12 bytes.
struct ModelSkinVertex
{
	uint joints_xy_; // joint0:16 | joint1:16
	uint joints_zw_; // joint2:16 | joint3:16
	uint weights_;   // 4 x 8-bit UNORM
};

struct ModelMeshlet
{
	uint vertex_offset_;
	uint triangle_offset_;
	uint vertex_count_;
	uint triangle_count_;
};

struct ModelMeshletBound
{
	float3 center_;
	float radius_;
	float3 cone_axis_;
	float cone_cutoff_;
};

struct ModelInstance
{
	row_major float4x4 world_;
	row_major float4x4 inverse_transpose_world_;

	// This instance's own world matrix as of the previous frame - used for
	// velocity (StaticModelMS.hlsl/SkeletalModelMS.hlsl) so moving/animated
	// objects get correct motion vectors instead of only reflecting camera
	// motion. Note: this captures the instance's overall transform only, not
	// per-bone skinning deformation.
	row_major float4x4 previous_world_;

	float4 base_color_;
	float metallic_;
	float roughness_;
	float alpha_cutoff_;
	float ior_;                     // KHR_materials_ior

	float3 emissive_;
	float emissive_strength_;       // KHR_materials_emissive_strength

	uint base_color_texture_index_;
	uint normal_texture_index_;
	uint metallic_roughness_texture_index_;
	uint emissive_texture_index_;

	uint vertex_buffer_index_;
	uint meshlet_buffer_index_;
	uint meshlet_bound_buffer_index_;
	uint vertex_indices_buffer_index_;

	uint primitive_indices_buffer_index_;
	uint meshlet_offset_;
	uint meshlet_count_;
	uint skin_index_;

	uint bone_offset_;
	uint double_sided_;
	uint blend_;
	uint selected_;

	// Cumulative screen-space LOD error of this instance's cluster, and the
	// error of the next coarser cluster in the same chain (FLT_MAX when there
	// is none or LOD selection must always keep this instance).
	float lod_error_;
	float lod_error_next_;
	float specular_factor_;         // KHR_materials_specular (factor)
	float clearcoat_factor_;        // KHR_materials_clearcoat (factor)

	float clearcoat_roughness_;     // KHR_materials_clearcoat (roughness)
	float anisotropy_;              // KHR_materials_anisotropy (strength)
	// Bindless SRV of the ModelSkinVertex buffer (0xFFFFFFFF for static).
	uint skin_vertex_buffer_index_;
	float model_instance_padding_3_;

	float3 specular_color_;         // KHR_materials_specular (color)
	float model_instance_padding_4_;

	// Dequantisation AABBs for CompressedModelVertex (per Crister).
	float3 position_min_;
	float texcoord_min_u_;

	float3 position_extent_;
	float texcoord_min_v_;

	float2 texcoord_extent_;
	float2 model_instance_padding_5_;
};

// Dequantises a CompressedModelVertex with the instance's AABBs. Must stay in
// exact sync with the CPU encode in Crister.cpp (Upload).
ModelVertex DecodeModelVertex(CompressedModelVertex packed, ModelInstance instance)
{
	ModelVertex vertex;

	float3 position01 = float3(
		packed.position_xy_ & 0xFFFF,
		packed.position_xy_ >> 16,
		packed.position_z_texu_ & 0xFFFF) / 65535.0;
	vertex.position_ = instance.position_min_ + position01 * instance.position_extent_;

	float2 texcoord01 = float2(
		packed.position_z_texu_ >> 16,
		packed.texv_tangent_ & 0xFFFF) / 65535.0;
	vertex.texcoord_ = float2(instance.texcoord_min_u_, instance.texcoord_min_v_) + texcoord01 * instance.texcoord_extent_;

	vertex.normal_ = OctNormalDecode(float2(packed.normal_ & 0xFFFF, packed.normal_ >> 16) / 65535.0);

	uint tangent_bits = packed.texv_tangent_ >> 16;
	float2 tangent_oct = float2(tangent_bits & 0xFF, (tangent_bits >> 8) & 0x7F) / float2(255.0, 127.0);
	float tangent_sign = (tangent_bits >> 15) ? 1.0 : -1.0;
	vertex.tangent_ = float4(OctNormalDecode(tangent_oct), tangent_sign);

	return vertex;
}

// Unpacks skinning attributes; weights are renormalised because four 8-bit
// UNORM values rarely sum to exactly one.
void DecodeModelSkinVertex(ModelSkinVertex packed, out uint4 joints, out float4 weights)
{
	joints = uint4(
		packed.joints_xy_ & 0xFFFF,
		packed.joints_xy_ >> 16,
		packed.joints_zw_ & 0xFFFF,
		packed.joints_zw_ >> 16);

	weights = float4(
		packed.weights_ & 0xFF,
		(packed.weights_ >> 8) & 0xFF,
		(packed.weights_ >> 16) & 0xFF,
		packed.weights_ >> 24) / 255.0;
	weights /= max(dot(weights, 1.0), 1e-6);
}

// Bone palette element (inverse bind matrix x joint global transform).
// The CPU stores engine Matrix (row-major, row-vector convention) via raw
// memcpy. Stored as four explicit row vectors instead of float4x4 so the
// layout does not depend on matrix-orientation annotations, which are
// unreliable inside structured buffers.
struct ModelBoneMatrix
{
	float4 row0_;
	float4 row1_;
	float4 row2_;
	float4 row3_;
};

// Rebuilds the row-vector-convention matrix (use as mul(vector, matrix)).
// The float4x4 vector constructor takes rows, so this is orientation-safe.
float4x4 LoadBoneMatrix(ModelBoneMatrix bone)
{
	return float4x4(bone.row0_, bone.row1_, bone.row2_, bone.row3_);
}

struct ModelASPayload
{
	uint instance_index;
	uint meshlet_indices[32];
};

struct ModelMSOutput
{
	float4 position         : SV_Position;
	float3 world_position   : POSITION0;
	float3 world_normal     : NORMAL0;
	float4 world_tangent    : TANGENT0;
	float2 texcoord         : TEXCOORD0;
	float4 current_position : TEXCOORD1;
	float4 previous_position: TEXCOORD2;
	nointerpolation uint instance_index : BLENDINDICES0;
	nointerpolation uint meshlet_index : BLENDINDICES1;
};

// Depth prepass output. Carries texcoord + instance index so the prepass pixel
// shader can apply the same alpha-cutout clip as the G-Buffer pass; without it
// masked (alphaMode=MASK) holes write depth and occlude geometry behind them.
struct DepthPrepassOutput
{
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
	nointerpolation uint instance_index : BLENDINDICES0;
};

struct ModelPSOutput
{
	float4 base_color_metallic     : SV_Target0;   // rgb: base color, a: metallic
	float4 normal_roughness        : SV_Target1;   // rg: oct normal, b: roughness, a: pack8x8(clearcoat_factor, clearcoat_roughness)
	float4 material_extension       : SV_Target2;   // r: anisotropy, gba: specular color
	float2 velocity                : SV_Target3;
	float4 emissive                : SV_Target4;   // emissive_strength baked in
};

#define OIT_MAX_LAYERS 4
#define OIT_INVALID_INDEX 0xFFFFFFFF

struct OITFragment
{
	uint packed_color_;
	float depth_;
	uint next_;
};

struct OITResolveOutput
{
	float4 position : SV_Position;
};

uint PackColorRGBA8(float4 color)
{
	return uint(saturate(color.r) * 255.0) |
		  (uint(saturate(color.g) * 255.0) << 8) |
		  (uint(saturate(color.b) * 255.0) << 16) |
		  (uint(saturate(color.a) * 255.0) << 24);
}

float4 UnpackColorRGBA8(uint packed)
{
	return float4(
		float(packed & 0xFF) / 255.0,
		float((packed >> 8) & 0xFF) / 255.0,
		float((packed >> 16) & 0xFF) / 255.0,
		float((packed >> 24) & 0xFF) / 255.0
	);
}
