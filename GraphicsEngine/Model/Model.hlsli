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

// Mirrors the C++ ShadingModel enum (Model/Crister.h). Read from
// ModelInstance::shading_model_ and switched on by
// Opaque/DeferredLightingPS.hlsl's direct-light dispatch.
#define SHADING_MODEL_PBR 0
#define SHADING_MODEL_UNLIT 1
#define SHADING_MODEL_PHONG 2
#define SHADING_MODEL_TOON 3

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
	float transmission_factor_;     // KHR_materials_transmission (factor)

	float3 specular_color_;         // KHR_materials_specular (color)
	float volume_thickness_factor_; // KHR_materials_volume (thickness factor)

	// Dequantisation AABBs for CompressedModelVertex (per Crister).
	float3 position_min_;
	float texcoord_min_u_;

	float3 position_extent_;
	float texcoord_min_v_;

	float2 texcoord_extent_;
	float volume_attenuation_distance_; // KHR_materials_volume (attenuation distance, FLT_MAX = none)
	float unlit_;                       // KHR_materials_unlit (1.0 = skip lighting)

	float3 volume_attenuation_color_;   // KHR_materials_volume (attenuation color)
	uint shading_model_;                // ShadingModel - see DeferredLightingPS.hlsl's dispatch

	float3 sheen_color_;                // KHR_materials_sheen (color)
	float sheen_roughness_;             // KHR_materials_sheen (roughness)

	float iridescence_factor_;          // KHR_materials_iridescence (factor)
	float iridescence_ior_;             // KHR_materials_iridescence (ior)
	float iridescence_thickness_;       // KHR_materials_iridescence (thickness, nm)
	float model_instance_padding_7_;

	// Raster morph blend (see ApplyMorphBlend below). morph_target_count_ ==
	// 0 disables blending entirely - always the case for a non-morphed
	// instance, and also for a morphed instance drawn from an own-page
	// (streamed-in, non-pool) cluster: only the LOD 0 shared vertex pool
	// range is index-aligned with morph_delta_buffer_index_/
	// vertex_morph_source_buffer_index_'s crister-wide numbering (see
	// Crister::vertexMorphSource_'s comment), so ModelRenderer leaves these
	// zeroed for any other cluster and that instance's coarser LOD renders
	// its frozen bind-pose shape instead of morphing.
	uint morph_delta_buffer_index_;
	uint vertex_morph_source_buffer_index_;
	uint morph_delta_offset_;      // float3 units, into morph_delta_buffer_index_
	uint morph_vertex_offset_;     // this SubMesh's Crister::vertices_ range start

	uint morph_vertex_count_;      // this SubMesh's ORIGINAL (pre-LOD) vertex count
	uint morph_target_count_;
	uint morph_weight_offset_;     // into the shared per-frame morph weight buffer
	uint model_instance_padding_8_;

	// KHR extension / occlusion textures (0xFFFFFFFF when absent). Sampled by
	// Model/Opaque/DeferredLightingPS.hlsl using the model UV recovered from the
	// VisibilityBuffer (RT4.zw) - the deferred pass has no interpolated
	// texcoord of its own. The matching scalar factors live further up; each
	// texture multiplies its factor per the glTF spec's channel convention.
	uint occlusion_texture_index_;              // .r  occlusion strength
	uint specular_texture_index_;               // .a  KHR_materials_specular (factor)
	uint specular_color_texture_index_;         // .rgb KHR_materials_specular (color)
	uint clearcoat_texture_index_;              // .r  KHR_materials_clearcoat (factor)

	uint clearcoat_roughness_texture_index_;    // .g  KHR_materials_clearcoat (roughness)
	uint clearcoat_normal_texture_index_;       // .rgb tangent-space clearcoat normal
	uint transmission_texture_index_;           // .r  KHR_materials_transmission
	uint thickness_texture_index_;              // .g  KHR_materials_volume (thickness)

	uint sheen_color_texture_index_;            // .rgb KHR_materials_sheen (color)
	uint sheen_roughness_texture_index_;        // .a  KHR_materials_sheen (roughness)
	uint iridescence_texture_index_;            // .r  KHR_materials_iridescence (factor)
	uint iridescence_thickness_texture_index_;  // .g  KHR_materials_iridescence (thickness lerp)

	uint anisotropy_texture_index_;             // .rg direction, .b strength
	float anisotropy_rotation_;                 // KHR_materials_anisotropy (rotation, radians)
	uint model_instance_padding_10_;
	uint model_instance_padding_11_;
};

// Blends morph_target_count_ morph targets into base_position, weighted by
// this frame's Animator-sampled weights. global_vertex_index must be the
// SAME index used to fetch this vertex's CompressedModelVertex (i.e. an
// index into the LOD 0 shared vertex pool - see morph_target_count_'s
// comment on why this is a no-op for any other cluster). No-op (returns
// base_position unchanged) when morph_target_count_ is 0. morph_weight_index
// is structured_indices.model_.morph_weight_index_, passed in rather than
// read directly so this header needs no additional includes (see
// ResolveModelSurface's bone_matrix_index parameter for the same reason).
float3 ApplyMorphBlend(float3 base_position, uint global_vertex_index, ModelInstance instance, uint morph_weight_index)
{
	if (instance.morph_target_count_ == 0)
	{
		return base_position;
	}

	StructuredBuffer<uint> vertex_morph_source = ResourceDescriptorHeap[instance.vertex_morph_source_buffer_index_];
	StructuredBuffer<float3> morph_deltas = ResourceDescriptorHeap[instance.morph_delta_buffer_index_];
	StructuredBuffer<float> morph_weights = ResourceDescriptorHeap[morph_weight_index];

	uint source_vertex_index = vertex_morph_source[global_vertex_index];
	uint local_vertex_index = source_vertex_index - instance.morph_vertex_offset_;

	float3 position = base_position;
	for (uint target = 0; target < instance.morph_target_count_; ++target)
	{
		float3 delta = morph_deltas[instance.morph_delta_offset_ + target * instance.morph_vertex_count_ + local_vertex_index];
		float weight = morph_weights[instance.morph_weight_offset_ + target];
		position += delta * weight;
	}
	return position;
}

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

// VisibilityBuffer groundwork: the G-Buffer pass PS (StaticModelPS/
// SkeletalModelPS) only needs texcoord (for the alpha-cutout clip) plus the
// id fields - world position/normal/tangent/velocity are no longer consumed
// here, so the MS doesn't compute or carry them anymore. Model/Material/MaterialResolveCS.hlsl
// recomputes all of that independently from (instance/meshlet/triangle) + the
// same raw vertex buffers.
struct ModelMSOutput
{
	float4 position         : SV_Position;
	float2 texcoord         : TEXCOORD0;
	nointerpolation uint instance_index : BLENDINDICES0;
	nointerpolation uint meshlet_index : BLENDINDICES1;
};

// Per-primitive Mesh Shader output (the "primitives" array, not "vertices") -
// SV_PrimitiveID is NOT auto-generated by the mesh shader pipeline stage the
// way it is for the traditional (non-indexed-draw) pipeline, so the
// triangle-in-meshlet index has to be passed explicitly like this.
struct ModelMSPrimitiveOutput
{
	nointerpolation uint triangle_in_meshlet_index : BLENDINDICES2;
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

// Raster G-Buffer pass output - just the VisibilityBuffer id (see
// GeometryBuffer::BeginVisibility). base_color/normal/roughness/material
// extension/velocity/emissive are no longer produced here; they're rewritten
// wholesale by the material resolve compute pass (Model/Material/MaterialResolveCS.hlsl)
// from this id + depth.
struct ModelPSOutput
{
	uint4 visibility_id            : SV_Target0;   // x: instance_index, y: pack(meshlet_index, triangle_in_meshlet_index), zw: asuint(texcoord)
};

// Packs a triangle's Visibility-Buffer identity plus the interpolated texcoord:
// y reserves the low 7 bits for triangle_in_meshlet_index (meshlets emit at most
// 124 triangles, so 0..123 fits) and the remaining high bits for meshlet_index.
// zw carry the rasterized texcoord bit-for-bit (asuint, not a narrowing pack) so
// the lighting pass can sample material textures without re-deriving UVs - the
// source UVs are UNORM16 within the mesh's UV AABB, so any narrowing here would
// lose precision that tiled UVs (large texcoord_extent_) actually need.
uint4 PackVisibilityID(uint instance_index, uint meshlet_index, uint triangle_in_meshlet_index, float2 texcoord)
{
	return uint4(instance_index, (meshlet_index << 7) | (triangle_in_meshlet_index & 0x7F), asuint(texcoord.x), asuint(texcoord.y));
}

void UnpackVisibilityID(uint4 packed, out uint instance_index, out uint meshlet_index, out uint triangle_in_meshlet_index)
{
	instance_index = packed.x;
	meshlet_index = packed.y >> 7;
	triangle_in_meshlet_index = packed.y & 0x7F;
}

// Texcoord half of the VisibilityBuffer id, as written by PackVisibilityID.
float2 UnpackVisibilityTexcoord(uint4 packed)
{
	return float2(asfloat(packed.z), asfloat(packed.w));
}

#define OIT_MAX_LAYERS 8
#define OIT_MAX_WALK 32
#define OIT_INVALID_INDEX 0xFFFFFFFF

// VisibilityBuffer material sort: pixels are bucketed by (instance_index %
// MATERIAL_SORT_BUCKET_COUNT) before Model/Material/MaterialResolveCS.hlsl runs, so
// adjacent threads in a wave land on the same/nearby instance and share
// texture indices - improving control-flow coherency and texture cache
// locality versus resolving in raw screen order. 1024 is also the max
// threadgroup size, so Model/Material/MaterialPrefixSumCS.hlsl's scan fits in one
// dispatch. See Model/Material/MaterialClassifyCS.hlsl / MaterialPrefixSumCS.hlsl /
// MaterialScatterCS.hlsl.
#define MATERIAL_SORT_BUCKET_COUNT 1024
#define MATERIAL_SORT_INVALID_PIXEL 0xFFFFFFFF

// 16 bytes - must match the fragmentStride in Model/Transparent/OITBuffer.cpp.
struct OITFragment
{
	uint packed_color_rg_;
	uint packed_color_ba_;
	float depth_;
	uint next_;
};

struct OITResolveOutput
{
	float4 position : SV_Position;
};

uint2 PackColorHalf4(float4 color)
{
	return uint2(
		f32tof16(color.r) | (f32tof16(color.g) << 16),
		f32tof16(color.b) | (f32tof16(color.a) << 16));
}

float4 UnpackColorHalf4(uint2 packed)
{
	return float4(
		f16tof32(packed.x & 0xFFFF),
		f16tof32(packed.x >> 16),
		f16tof32(packed.y & 0xFFFF),
		f16tof32(packed.y >> 16));
}

// Per-pixel surface rebuilt from a VisibilityBuffer-style
// (instance, meshlet, triangle-in-meshlet) triplet. Shared by
// Model/Material/MaterialResolveCS.hlsl (deferred opaque resolve, which gets the triplet
// by unpacking the VisID texture) and Model/Transparent/ModelTransparentPS.hlsl (OIT
// accumulation, which gets it straight from the rasterizer), so transparent and
// opaque geometry is always shaded from identical reconstructed attributes.
struct ModelSurface
{
	float3 world_position_;

	// The same point through instance.previous_world_ - the deferred resolve
	// needs it for motion vectors, the OIT path ignores it.
	float3 previous_world_position_;

	// World space, already flipped when the pixel lands on a back face.
	float3 normal_;
	float3 tangent_;
	float tangent_sign_;

	float2 texcoord_;

	// Decided from the world-space geometric face normal versus the view
	// direction rather than the NDC winding sign, so it does not depend on the
	// source model's handedness/winding convention.
	bool is_front_face_;
};

// pixel_ndc is the pixel centre in NDC (x right, y up, both in -1..1).
// bone_matrix_index/morph_weight_index are structured_indices.model_.
// bone_matrix_index_/morph_weight_index_, passed in rather than read
// directly so this header needs no additional includes.
ModelSurface ResolveModelSurface(ModelInstance instance, uint meshlet_index, uint triangle_in_meshlet_index, float2 pixel_ndc, row_major float4x4 view_projection, float3 camera_position, uint bone_matrix_index, uint morph_weight_index)
{
	StructuredBuffer<CompressedModelVertex> vertices = ResourceDescriptorHeap[instance.vertex_buffer_index_];
	StructuredBuffer<ModelMeshlet> meshlets = ResourceDescriptorHeap[instance.meshlet_buffer_index_];
	StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[instance.vertex_indices_buffer_index_];
	ByteAddressBuffer primitive_indices = ResourceDescriptorHeap[instance.primitive_indices_buffer_index_];

	ModelMeshlet meshlet = meshlets[meshlet_index];

	// Same 3-bytes-per-triangle unpack as StaticModelMS.hlsl / SkeletalModelMS.hlsl
	// (primitive_indices holds meshlet-local vertex numbers 0..63).
	uint byte_offset = meshlet.triangle_offset_ + triangle_in_meshlet_index * 3;
	uint aligned_offset = byte_offset & ~3;
	uint shift = (byte_offset & 3) * 8;

	uint dword0 = primitive_indices.Load(aligned_offset);
	uint packed = dword0 >> shift;
	if (shift > 8)
	{
		uint dword1 = primitive_indices.Load(aligned_offset + 4);
		packed |= dword1 << (32 - shift);
	}

	uint local_index0 = packed & 0xFF;
	uint local_index1 = (packed >> 8) & 0xFF;
	uint local_index2 = (packed >> 16) & 0xFF;

	uint global_index0 = vertex_indices[meshlet.vertex_offset_ + local_index0];
	uint global_index1 = vertex_indices[meshlet.vertex_offset_ + local_index1];
	uint global_index2 = vertex_indices[meshlet.vertex_offset_ + local_index2];

	ModelVertex vertex0 = DecodeModelVertex(vertices[global_index0], instance);
	ModelVertex vertex1 = DecodeModelVertex(vertices[global_index1], instance);
	ModelVertex vertex2 = DecodeModelVertex(vertices[global_index2], instance);

	float3 local_position0 = vertex0.position_;
	float3 local_position1 = vertex1.position_;
	float3 local_position2 = vertex2.position_;

	// Morph composes before skin (matches StaticModelMS.hlsl/
	// SkeletalModelMS.hlsl) - without this, the depth/VisID pass below
	// writes morphed positions while this reconstruction used the
	// unmorphed ones, and the mismatch corrupts the screen-space
	// barycentric reprojection entirely (wrong/garbage triangle shape).
	local_position0 = ApplyMorphBlend(local_position0, global_index0, instance, morph_weight_index);
	local_position1 = ApplyMorphBlend(local_position1, global_index1, instance, morph_weight_index);
	local_position2 = ApplyMorphBlend(local_position2, global_index2, instance, morph_weight_index);

	float3 local_normal0 = vertex0.normal_;
	float3 local_normal1 = vertex1.normal_;
	float3 local_normal2 = vertex2.normal_;
	float3 local_tangent0 = vertex0.tangent_.xyz;
	float3 local_tangent1 = vertex1.tangent_.xyz;
	float3 local_tangent2 = vertex2.tangent_.xyz;

	// Same linear blend skinning as SkeletalModelMS.hlsl.
	// instance.skin_index_ == 0xFFFFFFFF means static (unskinned).
	if (instance.skin_index_ != 0xFFFFFFFF)
	{
		StructuredBuffer<ModelBoneMatrix> bone_matrices = ResourceDescriptorHeap[bone_matrix_index];
		StructuredBuffer<ModelSkinVertex> skin_vertices = ResourceDescriptorHeap[instance.skin_vertex_buffer_index_];

		uint4 joints0, joints1, joints2;
		float4 weights0, weights1, weights2;
		DecodeModelSkinVertex(skin_vertices[global_index0], joints0, weights0);
		DecodeModelSkinVertex(skin_vertices[global_index1], joints1, weights1);
		DecodeModelSkinVertex(skin_vertices[global_index2], joints2, weights2);

		float4x4 skin_matrix0 =
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints0.x]) * weights0.x +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints0.y]) * weights0.y +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints0.z]) * weights0.z +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints0.w]) * weights0.w;
		float4x4 skin_matrix1 =
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints1.x]) * weights1.x +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints1.y]) * weights1.y +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints1.z]) * weights1.z +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints1.w]) * weights1.w;
		float4x4 skin_matrix2 =
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints2.x]) * weights2.x +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints2.y]) * weights2.y +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints2.z]) * weights2.z +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints2.w]) * weights2.w;

		local_position0 = mul(float4(local_position0, 1.0), skin_matrix0).xyz;
		local_position1 = mul(float4(local_position1, 1.0), skin_matrix1).xyz;
		local_position2 = mul(float4(local_position2, 1.0), skin_matrix2).xyz;
		local_normal0 = normalize(mul(float4(local_normal0, 0.0), skin_matrix0).xyz);
		local_normal1 = normalize(mul(float4(local_normal1, 0.0), skin_matrix1).xyz);
		local_normal2 = normalize(mul(float4(local_normal2, 0.0), skin_matrix2).xyz);
		local_tangent0 = normalize(mul(float4(local_tangent0, 0.0), skin_matrix0).xyz);
		local_tangent1 = normalize(mul(float4(local_tangent1, 0.0), skin_matrix1).xyz);
		local_tangent2 = normalize(mul(float4(local_tangent2, 0.0), skin_matrix2).xyz);
	}

	float3 world_position0 = mul(float4(local_position0, 1.0), instance.world_).xyz;
	float3 world_position1 = mul(float4(local_position1, 1.0), instance.world_).xyz;
	float3 world_position2 = mul(float4(local_position2, 1.0), instance.world_).xyz;

	float4 clip0 = mul(float4(world_position0, 1.0), view_projection);
	float4 clip1 = mul(float4(world_position1, 1.0), view_projection);
	float4 clip2 = mul(float4(world_position2, 1.0), view_projection);

	float2 ndc0 = clip0.xy / clip0.w;
	float2 ndc1 = clip1.xy / clip1.w;
	float2 ndc2 = clip2.xy / clip2.w;

	// Screen-space barycentric weights via edge functions.
	float area = (ndc1.x - ndc0.x) * (ndc2.y - ndc0.y) - (ndc1.y - ndc0.y) * (ndc2.x - ndc0.x);

	float edge0 = (ndc2.x - ndc1.x) * (pixel_ndc.y - ndc1.y) - (ndc2.y - ndc1.y) * (pixel_ndc.x - ndc1.x);
	float edge1 = (ndc0.x - ndc2.x) * (pixel_ndc.y - ndc2.y) - (ndc0.y - ndc2.y) * (pixel_ndc.x - ndc2.x);
	float edge2 = (ndc1.x - ndc0.x) * (pixel_ndc.y - ndc0.y) - (ndc1.y - ndc0.y) * (pixel_ndc.x - ndc0.x);
	float screen_w0 = edge0 / area;
	float screen_w1 = edge1 / area;
	float screen_w2 = edge2 / area;

	// Perspective correction: weighting the (NDC-linear) screen weights by invW
	// and renormalising makes a plain linear combination of attributes
	// perspective-correct.
	float inverse_w0 = 1.0 / clip0.w;
	float inverse_w1 = 1.0 / clip1.w;
	float inverse_w2 = 1.0 / clip2.w;
	float perspective_w0 = screen_w0 * inverse_w0;
	float perspective_w1 = screen_w1 * inverse_w1;
	float perspective_w2 = screen_w2 * inverse_w2;
	float perspective_sum = perspective_w0 + perspective_w1 + perspective_w2;
	perspective_w0 /= perspective_sum;
	perspective_w1 /= perspective_sum;
	perspective_w2 /= perspective_sum;

	ModelSurface surface;
	surface.world_position_ = perspective_w0 * world_position0 + perspective_w1 * world_position1 + perspective_w2 * world_position2;

	float3 previous_world_position0 = mul(float4(local_position0, 1.0), instance.previous_world_).xyz;
	float3 previous_world_position1 = mul(float4(local_position1, 1.0), instance.previous_world_).xyz;
	float3 previous_world_position2 = mul(float4(local_position2, 1.0), instance.previous_world_).xyz;
	surface.previous_world_position_ = perspective_w0 * previous_world_position0 + perspective_w1 * previous_world_position1 + perspective_w2 * previous_world_position2;

	float3 N = normalize(perspective_w0 * local_normal0 + perspective_w1 * local_normal1 + perspective_w2 * local_normal2);
	float3 world_tangent_xyz = normalize(perspective_w0 * local_tangent0 + perspective_w1 * local_tangent1 + perspective_w2 * local_tangent2);

	surface.tangent_sign_ = vertex0.tangent_.w;
	surface.texcoord_ = perspective_w0 * vertex0.texcoord_ + perspective_w1 * vertex1.texcoord_ + perspective_w2 * vertex2.texcoord_;

	// Normals go through the inverse transpose, tangents through the plain
	// world matrix - same as StaticModelMS.hlsl used to do.
	N = normalize(mul(float4(N, 0.0), instance.inverse_transpose_world_).xyz);
	surface.tangent_ = normalize(mul(float4(world_tangent_xyz, 0.0), instance.world_).xyz);

	// Facing from the world-space geometric face normal versus the view
	// direction, NOT the NDC winding sign (which depends on the source model's
	// handedness). The face normal's sign is vertex-order dependent, so align it
	// with the interpolated shading normal first.
	float3 geometric_face_normal = cross(world_position1 - world_position0, world_position2 - world_position0);
	if (dot(geometric_face_normal, N) < 0.0)
	{
		geometric_face_normal = -geometric_face_normal;
	}
	float3 view_direction_for_facing = normalize(camera_position - surface.world_position_);
	surface.is_front_face_ = dot(geometric_face_normal, view_direction_for_facing) > 0.0;

	if (!surface.is_front_face_)
	{
		N = -N;
	}
	surface.normal_ = N;

	return surface;
}
