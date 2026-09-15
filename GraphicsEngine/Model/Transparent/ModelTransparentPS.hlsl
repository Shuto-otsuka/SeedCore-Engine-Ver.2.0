#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/UnorderedAccesses.hlsli"
#include "../../Shader/Constants.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Scene.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Light/Light.hlsli"
#include "../../Light/Cluster.hlsli"
#include "../../Light/BidirectionalReflectanceDistributionFunction.hlsli"
#include "../../Raytracing/Reflection/Reflection.hlsli"
#include "../../Sky/SkyGenerate.hlsli"
#include "OitShading.hlsli"

float3 EvalTransparentDirectLight(float3 normal, float3 view, float3 light_direction, float3 diffuse_color, float3 f0, float roughness)
{
	float normal_dot_light = max(dot(normal, light_direction), 0.0);
	if (normal_dot_light <= 0.0)
	{
		return float3(0, 0, 0);
	}

	float normal_dot_view = max(dot(normal, view), 0.001);
	float3 half_vector = normalize(view + light_direction);
	float normal_dot_half = max(dot(normal, half_vector), 0.0);
	float view_dot_half = max(dot(view, half_vector), 0.0);

	float alpha_roughness = roughness * roughness;
	float3 f90 = float3(1.0, 1.0, 1.0);

	float3 diffuse = BrdfLambertian(f0, f90, diffuse_color, view_dot_half);
	float3 specular = BrdfSpecularGgx(f0, f90, alpha_roughness, view_dot_half, normal_dot_light, normal_dot_view, normal_dot_half);

	return (diffuse + specular) * normal_dot_light;
}

/**
* [EN]
* Pixel Shader for transparent model PPLL accumulation.
* Instead of writing to G-Buffer render targets, this PS allocates
* a fragment in the Per-Pixel Linked List and writes color + depth.
* Used by both static and skeletal transparent models.
*
* The surface is fully lit here rather than written out flat: transparent
* geometry never reaches the deferred G-Buffer (it has no VisibilityBuffer
* entry, since the OIT pass writes no render targets), so
* Model/DeferredLightingPS.hlsl never gets a chance to shade it. Shading
* therefore happens inline, reconstructing the surface with the same logic
* Model/Material/MaterialResolveCS.hlsl's deferred resolve uses so transparent and
* opaque geometry are shaded from identical attributes. The lighting model is
* the same base PBR as the deferred path (Lambert + GGX for the directional
* light AND for every clustered punctual light, diffuse/specular IBL); the
* extras that only exist per-instance in the deferred path (clearcoat/sheen/
* iridescence, ray-traced shadows/AO/GI) are deliberately left out - they need
* G-Buffer-resident screen-space inputs that transparent fragments do not have.
*
* The head pointer texture, fragment buffer, and counter are accessed
* as UAVs via the bindless descriptor heap.
*
* [earlydepthstencil] is mandatory, not an optimization: a pixel shader that
* writes UAVs and calls clip() runs with late depth-stencil by default, so the
* fragment would be appended to the linked list before the depth test, and the
* PSO has no render target and no depth write for a failed test to suppress.
*
* ---------------------------------------------------------------------
*
* [JP]
* 透明モデル PPLL 蓄積用のピクセルシェーダー。
* G-Buffer RT に書き込む代わりに、Per-Pixel Linked List にフラグメントを
* アトミックに割り当て、色 + 深度を書き込む。
* 静的・スケルタル両方の透明モデルで使用する。
*
* ここで面を完全にライティングしてから書き込む(以前のようにフラットな
* ベースカラーをそのまま書かない): 透明ジオメトリは deferred の G-Buffer に
* 一切載らない(OITパスはRTを書かないので VisibilityBuffer のエントリが無い)
* ため、Model/DeferredLightingPS.hlsl が陰影を付ける機会が無い。そこで
* この場でシェーディングし、面の再構築には Model/Material/MaterialResolveCS.hlsl の
* deferred 解決パスと同じロジックを使う — 透明面と不透明面が必ず同一の
* 属性から陰影付けされるようにするため。ライティングモデルは deferred と
* 同じ基本PBR(ディレクショナルライトにもクラスタ済みパンクチュアルライト
* にも Lambert + GGX、拡散/鏡面IBL)。deferred 側だけにある追加要素
* (クリアコート/シーン/虹彩、レイトレの影/AO/GI)はあえて省いている —
* それらは透明フラグメントが持ち得ない
* G-Buffer 常駐のスクリーン空間入力を必要とするため。
*
* ヘッドポインタテクスチャ、フラグメントバッファ、カウンターは
* バインドレスディスクリプタヒープ経由で UAV としてアクセスする。
*
* [earlydepthstencil] は最適化ではなく必須。UAV に書き込み clip() を持つ
* ピクセルシェーダは既定で後段深度ステンシルになるため、深度テストより先に
* リンクリストへ追加されてしまい、この PSO には落ちたテストが抑止できる
* レンダーターゲットも深度書き込みも無い。
*/
[earlydepthstencil]
void main(ModelMSOutput input, ModelMSPrimitiveOutput primitive)
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	ModelStructuredBuffer instance = instances[input.instance_index];

	/// [JP] まずアルファだけ先に評価してクリップする — 完全に透明なフラグメント
	///      に対して面の再構築とライティングを走らせても無駄なので、
	///      ベースカラーのサンプルは補間済み texcoord で先に済ませる。
	float4 base_color = instance.texture_.base_color_;
	if (instance.texture_.base_color_texture_index_ != OIT_INVALID_INDEX)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.texture_.base_color_texture_index_];
		base_color *= base_color_texture.Sample(sampler_aniso_wrap, input.texcoord);
	}

	clip(base_color.a - instance.texture_.alpha_cutoff_);

	SceneConstantBuffer scene = GetSceneConstantBuffer();

	/// [JP] SV_Position.xy は既にピクセル中心(x+0.5)なのでそのまま NDC へ。
	float2 uv = input.position.xy * scene.inverse_screen_size_;
	float2 pixel_ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

	StructuredBuffer<CompressedModelVertex> vertices = ResourceDescriptorHeap[instance.geometry_.vertex_buffer_index_];
	StructuredBuffer<ModelMeshlet> meshlets = ResourceDescriptorHeap[instance.geometry_.meshlet_buffer_index_];
	StructuredBuffer<uint> resolve_vertex_indices = ResourceDescriptorHeap[instance.geometry_.vertex_indices_buffer_index_];
	ByteAddressBuffer resolve_primitive_indices = ResourceDescriptorHeap[instance.geometry_.primitive_indices_buffer_index_];

	ModelMeshlet resolve_meshlet = meshlets[input.meshlet_index];

	// Same 3-bytes-per-triangle unpack as StaticModelMS.hlsl / SkeletalModelMS.hlsl
	// (primitive_indices holds meshlet-local vertex numbers 0..63).
	uint resolve_byte_offset = resolve_meshlet.triangle_offset_ + primitive.triangle_in_meshlet_index * 3;
	uint resolve_aligned_offset = resolve_byte_offset & ~3;
	uint resolve_shift = (resolve_byte_offset & 3) * 8;

	uint resolve_dword0 = resolve_primitive_indices.Load(resolve_aligned_offset);
	uint resolve_packed = resolve_dword0 >> resolve_shift;
	if (resolve_shift > 8)
	{
		uint resolve_dword1 = resolve_primitive_indices.Load(resolve_aligned_offset + 4);
		resolve_packed |= resolve_dword1 << (32 - resolve_shift);
	}

	uint local_index0 = resolve_packed & 0xFF;
	uint local_index1 = (resolve_packed >> 8) & 0xFF;
	uint local_index2 = (resolve_packed >> 16) & 0xFF;

	uint global_index0 = resolve_vertex_indices[resolve_meshlet.vertex_offset_ + local_index0];
	uint global_index1 = resolve_vertex_indices[resolve_meshlet.vertex_offset_ + local_index1];
	uint global_index2 = resolve_vertex_indices[resolve_meshlet.vertex_offset_ + local_index2];

	ExpandedModelVertex vertex0 = DecodeModelVertex(vertices[global_index0], instance);
	ExpandedModelVertex vertex1 = DecodeModelVertex(vertices[global_index1], instance);
	ExpandedModelVertex vertex2 = DecodeModelVertex(vertices[global_index2], instance);

	float3 local_position0 = vertex0.position_;
	float3 local_position1 = vertex1.position_;
	float3 local_position2 = vertex2.position_;
	float3 previous_local_position0 = local_position0;
	float3 previous_local_position1 = local_position1;
	float3 previous_local_position2 = local_position2;

	// Morph composes before skin (matches StaticModelMS.hlsl/
	// SkeletalModelMS.hlsl) - without this, the depth/VisID pass below
	// writes morphed positions while this reconstruction used the
	// unmorphed ones, and the mismatch corrupts the screen-space
	// barycentric reprojection entirely (wrong/garbage triangle shape).
	// global_vertex_index must be the SAME index used to fetch the vertex's
	// CompressedModelVertex (i.e. an index into the LOD 0 shared vertex pool
	// - see morph_target_count_'s comment for why this is a no-op for any
	// other cluster).
	if (instance.morph_.morph_target_count_ != 0)
	{
		StructuredBuffer<uint> vertex_morph_source = ResourceDescriptorHeap[instance.morph_.vertex_morph_source_buffer_index_];
		StructuredBuffer<float3> morph_deltas = ResourceDescriptorHeap[instance.morph_.morph_delta_buffer_index_];
		StructuredBuffer<float> morph_weights = ResourceDescriptorHeap[shader_resource_indices.model_.morph_weight_index_];
		StructuredBuffer<float> previous_morph_weights = ResourceDescriptorHeap[shader_resource_indices.model_.previous_morph_weight_index_];

		uint local_vertex_index0 = vertex_morph_source[global_index0] - instance.morph_.morph_vertex_offset_;
		uint local_vertex_index1 = vertex_morph_source[global_index1] - instance.morph_.morph_vertex_offset_;
		uint local_vertex_index2 = vertex_morph_source[global_index2] - instance.morph_.morph_vertex_offset_;

		for (uint target = 0; target < instance.morph_.morph_target_count_; ++target)
		{
			float weight = morph_weights[instance.morph_.morph_weight_offset_ + target];
			float previous_weight = previous_morph_weights[instance.morph_.morph_weight_offset_ + target];
			float3 morph_delta0 = morph_deltas[instance.morph_.morph_delta_offset_ + target * instance.morph_.morph_vertex_count_ + local_vertex_index0];
			float3 morph_delta1 = morph_deltas[instance.morph_.morph_delta_offset_ + target * instance.morph_.morph_vertex_count_ + local_vertex_index1];
			float3 morph_delta2 = morph_deltas[instance.morph_.morph_delta_offset_ + target * instance.morph_.morph_vertex_count_ + local_vertex_index2];
			local_position0 += morph_delta0 * weight;
			local_position1 += morph_delta1 * weight;
			local_position2 += morph_delta2 * weight;
			previous_local_position0 += morph_delta0 * previous_weight;
			previous_local_position1 += morph_delta1 * previous_weight;
			previous_local_position2 += morph_delta2 * previous_weight;
		}
	}

	float3 local_normal0 = vertex0.normal_;
	float3 local_normal1 = vertex1.normal_;
	float3 local_normal2 = vertex2.normal_;
	float3 local_tangent0 = vertex0.tangent_.xyz;
	float3 local_tangent1 = vertex1.tangent_.xyz;
	float3 local_tangent2 = vertex2.tangent_.xyz;

	// Same linear blend skinning as SkeletalModelMS.hlsl.
	// instance.skining_.skin_index_ == 0xFFFFFFFF means static (unskinned).
	if (instance.skining_.skin_index_ != 0xFFFFFFFF)
	{
		StructuredBuffer<ModelBoneMatrix> bone_matrices = ResourceDescriptorHeap[shader_resource_indices.model_.bone_matrix_index_];
		StructuredBuffer<CompressedModelSkin> skin_vertices = ResourceDescriptorHeap[instance.skining_.skin_vertex_buffer_index_];

		ExpandedModelSkin skin0 = DecodeSkinVertex(skin_vertices[global_index0]);
		ExpandedModelSkin skin1 = DecodeSkinVertex(skin_vertices[global_index1]);
		ExpandedModelSkin skin2 = DecodeSkinVertex(skin_vertices[global_index2]);

		float4x4 skin_matrix0 =
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.x]) * skin0.weights_.x +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.y]) * skin0.weights_.y +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.z]) * skin0.weights_.z +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.w]) * skin0.weights_.w;
		float4x4 skin_matrix1 =
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.x]) * skin1.weights_.x +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.y]) * skin1.weights_.y +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.z]) * skin1.weights_.z +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.w]) * skin1.weights_.w;
		float4x4 skin_matrix2 =
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.x]) * skin2.weights_.x +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.y]) * skin2.weights_.y +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.z]) * skin2.weights_.z +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.w]) * skin2.weights_.w;

		StructuredBuffer<ModelBoneMatrix> previous_bone_matrices = ResourceDescriptorHeap[shader_resource_indices.model_.previous_bone_matrix_index_];
		float4x4 previous_skin_matrix0 =
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.x]) * skin0.weights_.x +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.y]) * skin0.weights_.y +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.z]) * skin0.weights_.z +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.w]) * skin0.weights_.w;
		float4x4 previous_skin_matrix1 =
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.x]) * skin1.weights_.x +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.y]) * skin1.weights_.y +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.z]) * skin1.weights_.z +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.w]) * skin1.weights_.w;
		float4x4 previous_skin_matrix2 =
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.x]) * skin2.weights_.x +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.y]) * skin2.weights_.y +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.z]) * skin2.weights_.z +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.w]) * skin2.weights_.w;

		local_position0 = mul(float4(local_position0, 1.0), skin_matrix0).xyz;
		local_position1 = mul(float4(local_position1, 1.0), skin_matrix1).xyz;
		local_position2 = mul(float4(local_position2, 1.0), skin_matrix2).xyz;
		previous_local_position0 = mul(float4(previous_local_position0, 1.0), previous_skin_matrix0).xyz;
		previous_local_position1 = mul(float4(previous_local_position1, 1.0), previous_skin_matrix1).xyz;
		previous_local_position2 = mul(float4(previous_local_position2, 1.0), previous_skin_matrix2).xyz;
		local_normal0 = normalize(mul(float4(local_normal0, 0.0), skin_matrix0).xyz);
		local_normal1 = normalize(mul(float4(local_normal1, 0.0), skin_matrix1).xyz);
		local_normal2 = normalize(mul(float4(local_normal2, 0.0), skin_matrix2).xyz);
		local_tangent0 = normalize(mul(float4(local_tangent0, 0.0), skin_matrix0).xyz);
		local_tangent1 = normalize(mul(float4(local_tangent1, 0.0), skin_matrix1).xyz);
		local_tangent2 = normalize(mul(float4(local_tangent2, 0.0), skin_matrix2).xyz);
	}

	float3 world_position0 = mul(float4(local_position0, 1.0), instance.transform_.world_).xyz;
	float3 world_position1 = mul(float4(local_position1, 1.0), instance.transform_.world_).xyz;
	float3 world_position2 = mul(float4(local_position2, 1.0), instance.transform_.world_).xyz;

	float4 resolve_clip0 = mul(float4(world_position0, 1.0), scene.current_view_projection_);
	float4 resolve_clip1 = mul(float4(world_position1, 1.0), scene.current_view_projection_);
	float4 resolve_clip2 = mul(float4(world_position2, 1.0), scene.current_view_projection_);

	float2 resolve_ndc0 = resolve_clip0.xy / resolve_clip0.w;
	float2 resolve_ndc1 = resolve_clip1.xy / resolve_clip1.w;
	float2 resolve_ndc2 = resolve_clip2.xy / resolve_clip2.w;

	// Screen-space barycentric weights via edge functions.
	float resolve_area = (resolve_ndc1.x - resolve_ndc0.x) * (resolve_ndc2.y - resolve_ndc0.y) - (resolve_ndc1.y - resolve_ndc0.y) * (resolve_ndc2.x - resolve_ndc0.x);

	float resolve_edge0 = (resolve_ndc2.x - resolve_ndc1.x) * (pixel_ndc.y - resolve_ndc1.y) - (resolve_ndc2.y - resolve_ndc1.y) * (pixel_ndc.x - resolve_ndc1.x);
	float resolve_edge1 = (resolve_ndc0.x - resolve_ndc2.x) * (pixel_ndc.y - resolve_ndc2.y) - (resolve_ndc0.y - resolve_ndc2.y) * (pixel_ndc.x - resolve_ndc2.x);
	float resolve_edge2 = (resolve_ndc1.x - resolve_ndc0.x) * (pixel_ndc.y - resolve_ndc0.y) - (resolve_ndc1.y - resolve_ndc0.y) * (pixel_ndc.x - resolve_ndc0.x);
	float screen_w0 = resolve_edge0 / resolve_area;
	float screen_w1 = resolve_edge1 / resolve_area;
	float screen_w2 = resolve_edge2 / resolve_area;

	// Perspective correction: weighting the (NDC-linear) screen weights by invW
	// and renormalising makes a plain linear combination of attributes
	// perspective-correct.
	float inverse_w0 = 1.0 / resolve_clip0.w;
	float inverse_w1 = 1.0 / resolve_clip1.w;
	float inverse_w2 = 1.0 / resolve_clip2.w;
	float perspective_w0 = screen_w0 * inverse_w0;
	float perspective_w1 = screen_w1 * inverse_w1;
	float perspective_w2 = screen_w2 * inverse_w2;
	float perspective_sum = perspective_w0 + perspective_w1 + perspective_w2;
	perspective_w0 /= perspective_sum;
	perspective_w1 /= perspective_sum;
	perspective_w2 /= perspective_sum;

	ModelSurface surface;
	surface.world_position_ = perspective_w0 * world_position0 + perspective_w1 * world_position1 + perspective_w2 * world_position2;

	float3 resolve_previous_world_position0 = mul(float4(previous_local_position0, 1.0), instance.transform_.previous_world_).xyz;
	float3 resolve_previous_world_position1 = mul(float4(previous_local_position1, 1.0), instance.transform_.previous_world_).xyz;
	float3 resolve_previous_world_position2 = mul(float4(previous_local_position2, 1.0), instance.transform_.previous_world_).xyz;
	surface.previous_world_position_ = perspective_w0 * resolve_previous_world_position0 + perspective_w1 * resolve_previous_world_position1 + perspective_w2 * resolve_previous_world_position2;

	float3 resolve_N = normalize(perspective_w0 * local_normal0 + perspective_w1 * local_normal1 + perspective_w2 * local_normal2);
	float3 resolve_world_tangent_xyz = normalize(perspective_w0 * local_tangent0 + perspective_w1 * local_tangent1 + perspective_w2 * local_tangent2);

	surface.tangent_sign_ = vertex0.tangent_.w;
	surface.texcoord_ = perspective_w0 * vertex0.texcoord_ + perspective_w1 * vertex1.texcoord_ + perspective_w2 * vertex2.texcoord_;

	// Normals go through the inverse transpose, tangents through the plain
	// world matrix - same as StaticModelMS.hlsl used to do.
	resolve_N = normalize(mul(float4(resolve_N, 0.0), instance.transform_.inverse_transpose_world_).xyz);
	surface.tangent_ = normalize(mul(float4(resolve_world_tangent_xyz, 0.0), instance.transform_.world_).xyz);

	// Facing from the world-space geometric face normal versus the view
	// direction, NOT the NDC winding sign (which depends on the source model's
	// handedness). The face normal's sign is vertex-order dependent, so align it
	// with the interpolated shading normal first.
	float3 resolve_geometric_face_normal = cross(world_position1 - world_position0, world_position2 - world_position0);
	if (dot(resolve_geometric_face_normal, resolve_N) < 0.0)
	{
		resolve_geometric_face_normal = -resolve_geometric_face_normal;
	}
	if (instance.shading_.shading_model_ == SHADING_MODEL_FLAT)
	{
		resolve_N = normalize(resolve_geometric_face_normal);
	}
	float3 resolve_view_direction_for_facing = normalize(scene.camera_position_.xyz - surface.world_position_);
	bool resolve_is_front_face = dot(resolve_geometric_face_normal, resolve_view_direction_for_facing) > 0.0;

	if (!resolve_is_front_face)
	{
		resolve_N = -resolve_N;
	}
	surface.normal_ = resolve_N;

	float3 N = surface.normal_;

	if (instance.texture_.normal_texture_index_ != OIT_INVALID_INDEX)
	{
		float3 T = surface.tangent_;
		float3 B = cross(N, T) * surface.tangent_sign_;
		float3x3 TBN = float3x3(T, B, N);

		Texture2D normal_texture = ResourceDescriptorHeap[instance.texture_.normal_texture_index_];
		float3 normal_sample = normal_texture.Sample(sampler_linear_wrap, surface.texcoord_).xyz * 2.0 - 1.0;
		N = normalize(mul(normal_sample, TBN));
	}

	float metallic = instance.texture_.metallic_;
	float roughness = instance.texture_.roughness_;
	if (instance.texture_.metallic_roughness_texture_index_ != OIT_INVALID_INDEX)
	{
		Texture2D metallic_roughness_texture = ResourceDescriptorHeap[instance.texture_.metallic_roughness_texture_index_];
		float4 metallic_roughness = metallic_roughness_texture.Sample(sampler_linear_wrap, surface.texcoord_);
		metallic *= metallic_roughness.b;
		roughness *= metallic_roughness.g;
	}
	roughness = clamp(roughness, 0.045, 1.0);

	float3 emissive = instance.texture_.emissive_;
	if (instance.texture_.emissive_texture_index_ != OIT_INVALID_INDEX)
	{
		Texture2D emissive_texture = ResourceDescriptorHeap[instance.texture_.emissive_texture_index_];
		emissive *= emissive_texture.Sample(sampler_linear_wrap, surface.texcoord_).rgb;
	}
	emissive *= instance.texture_.emissive_strength_;

	/// [JP] KHR_materials_unlit: ライティングを丸ごと飛ばして
	///      base_color + emissive をそのまま使う(DeferredLightingPS.hlsl と同じ)。
	float3 lighting;
	if (instance.extension_.unlit_ > 0.5)
	{
		lighting = base_color.rgb + emissive;
	}
	else
	{
		float3 view = normalize(scene.camera_position_.xyz - surface.world_position_);

		/// [JP] DeferredLightingPS.hlsl と同じ誘電体F0の組み立て
		///      (KHR_materials_ior + KHR_materials_specular)。
		float dielectric = (instance.texture_.ior_ - 1.0) / (instance.texture_.ior_ + 1.0);
		dielectric = dielectric * dielectric;
		float3 dielectric_f0 = saturate(dielectric * instance.extension_.specular_color_ * instance.extension_.specular_factor_);

		float3 diffuse_color = base_color.rgb * (1.0 - metallic);
		float3 f0 = lerp(dielectric_f0, base_color.rgb, metallic);

		lighting = float3(0, 0, 0);

		ConstantBuffer<LightConstantBuffer> light_constant_buffer = ResourceDescriptorHeap[constant_indices.light_index_];

		/// [JP] ディレクショナルライト。影レイは撃たない — 透明フラグメントは
		///      スクリーン空間の影バッファ(不透明面の深度で解決済み)を引けないため。
		if (GetDirectionalLightConstantBuffer().sun_intensity_ > 0.0)
		{
			float3 light_direction = normalize(-GetDirectionalLightConstantBuffer().direction_);
			lighting += EvalTransparentDirectLight(N, view, light_direction, diffuse_color, f0, roughness) * GetDirectionalLightConstantBuffer().sun_color_.rgb * GetDirectionalLightConstantBuffer().sun_intensity_;
		}

		/// [JP] Point/Spot/Rect。DeferredLightingPS.hlsl と同じクラスタ走査 +
		///      同じ BRDF。Reflection.hlsli の ComputeClusteredPunctualLighting は
		///      BRDF を通さない放射照度を返す仕様なので使わない。
		{
			float4 view_position = mul(float4(surface.world_position_, 1.0), scene.view_);
			float linear_depth = view_position.z;

			uint2 cluster_pixel = uint2(input.position.xy);
			uint3 cluster_count = uint3(light_constant_buffer.cluster_count_x_, light_constant_buffer.cluster_count_y_, CLUSTER_DEPTH_SLICES);
			uint tile_x = cluster_pixel.x / CLUSTER_TILE_SIZE;
			uint tile_y = cluster_pixel.y / CLUSTER_TILE_SIZE;
			uint slice = ComputeDepthSlice(linear_depth, scene.near_plane_, scene.far_plane_);
			uint cluster_index = ClusterIndex(uint3(tile_x, tile_y, slice), cluster_count);

			StructuredBuffer<ClusterInstance> cluster_data = ResourceDescriptorHeap[shader_resource_indices.light_.cluster_data_index_];
			ByteAddressBuffer light_list = ResourceDescriptorHeap[shader_resource_indices.light_.cluster_light_list_index_];

			ClusterInstance cluster = cluster_data[cluster_index];
			uint base = cluster_index * CLUSTER_STRIDE;

			if (cluster.point_count_ > 0)
			{
				StructuredBuffer<PointLightStructuredBuffer> point_lights = GetPointLightStructuredBuffer(shader_resource_indices.light_.point_light_index_);
				uint count = min(cluster.point_count_, CLUSTER_MAX_POINT_LIGHTS);

				for (uint index = 0; index < count; index++)
				{
					uint light_index = light_list.Load((base + index) * 4);
					PointLightStructuredBuffer point_light = point_lights[light_index];

					float3 to_light = point_light.position_ - surface.world_position_;
					float distance_to_light = length(to_light);
					float3 light_direction = to_light / max(distance_to_light, 0.0001);
					float attenuation_ratio = saturate(distance_to_light / point_light.range_);
					float attenuation_ratio2 = attenuation_ratio * attenuation_ratio;
					float attenuation = saturate(1.0 - attenuation_ratio2 * attenuation_ratio2);
					attenuation = attenuation * attenuation / max(distance_to_light * distance_to_light, 0.0001);

					float3 light_color = point_light.color_.rgb * point_light.intensity_ * attenuation;
					lighting += EvalTransparentDirectLight(N, view, light_direction, diffuse_color, f0, roughness) * light_color;
				}
			}

			if (cluster.spot_count_ > 0)
			{
				StructuredBuffer<SpotLightStructuredBuffer> spot_lights = GetSpotLightStructuredBuffer(shader_resource_indices.light_.spot_light_index_);
				uint count = min(cluster.spot_count_, CLUSTER_MAX_SPOT_LIGHTS);

				for (uint index = 0; index < count; index++)
				{
					uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + index) * 4);
					SpotLightStructuredBuffer spot_light = spot_lights[light_index];

					float3 to_light = spot_light.position_ - surface.world_position_;
					float distance_to_light = length(to_light);
					float3 light_direction = to_light / max(distance_to_light, 0.0001);

					float cos_angle = dot(-light_direction, spot_light.direction_);
					float spot_fade = saturate((cos_angle - spot_light.cos_half_angle_) / max(spot_light.softness_ * (1.0 - spot_light.cos_half_angle_), 0.0001));

					float attenuation_ratio = saturate(distance_to_light / spot_light.range_);
					float attenuation_ratio2 = attenuation_ratio * attenuation_ratio;
					float attenuation = saturate(1.0 - attenuation_ratio2 * attenuation_ratio2);
					attenuation = attenuation * attenuation / max(distance_to_light * distance_to_light, 0.0001) * spot_fade;

					float3 light_color = spot_light.color_.rgb * spot_light.intensity_ * attenuation;
					lighting += EvalTransparentDirectLight(N, view, light_direction, diffuse_color, f0, roughness) * light_color;
				}
			}

			if (cluster.rect_count_ > 0)
			{
				StructuredBuffer<RectLightStructuredBuffer> rect_lights = GetRectLightStructuredBuffer(shader_resource_indices.light_.rect_light_index_);
				uint count = min(cluster.rect_count_, CLUSTER_MAX_RECT_LIGHTS);

				for (uint index = 0; index < count; index++)
				{
					uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + index) * 4);
					RectLightStructuredBuffer rect_light = rect_lights[light_index];

					if (dot(surface.world_position_ - rect_light.position_, rect_light.normal_) <= 0.0)
					{
						continue;
					}

					float3 rect_delta = surface.world_position_ - rect_light.position_;
					float rect_local_x = clamp(dot(rect_delta, rect_light.right_), -rect_light.half_width_, rect_light.half_width_);
					float rect_local_y = clamp(dot(rect_delta, rect_light.up_), -rect_light.half_height_, rect_light.half_height_);
					float3 representative_point = rect_light.position_ + rect_light.right_ * rect_local_x + rect_light.up_ * rect_local_y;
					float3 to_light = representative_point - surface.world_position_;
					float distance_to_light = length(to_light);
					float3 light_direction = to_light / max(distance_to_light, 0.0001);

					float attenuation_ratio = saturate(distance_to_light / rect_light.range_);
					float attenuation_ratio2 = attenuation_ratio * attenuation_ratio;
					float attenuation = saturate(1.0 - attenuation_ratio2 * attenuation_ratio2);
					attenuation = attenuation * attenuation / max(distance_to_light * distance_to_light, 0.0001);
					float3 light_color = rect_light.color_.rgb * rect_light.intensity_ * attenuation;
					lighting += EvalTransparentDirectLight(N, view, light_direction, diffuse_color, f0, roughness) * light_color;
				}
			}
		}

		/// [JP] IBL。スカイマップが無い場合の環境光は0(DeferredLightingPS.hlsl
		///      と同じ扱い) - 直接光だけがこのピクセルを照らす。
		if (shader_resource_indices.sky_.diffuse_irradiance_index_ != 0)
		{
			lighting += ImageBasedLightingRadianceLambertian(N, view, roughness, diffuse_color, f0) * GetSkyConstantBuffer().intensity_;
			lighting += ImageBasedLightingRadianceGgx(N, view, roughness, f0) * GetSkyConstantBuffer().intensity_;
		}

		lighting += emissive;
	}

	float4 final_color = float4(lighting, base_color.a);

	RWTexture2D<uint> head_pointer = ResourceDescriptorHeap[unordered_access_indices.oit_.head_pointer_index_];
	RWStructuredBuffer<OITFragment> fragment_buffer = ResourceDescriptorHeap[unordered_access_indices.oit_.fragment_buffer_index_];
	RWByteAddressBuffer oit_counter = ResourceDescriptorHeap[unordered_access_indices.oit_.counter_index_];

	uint max_fragments = GetOitConstantBuffer().fragment_capacity_;

	uint new_index;
	oit_counter.InterlockedAdd(0, 1, new_index);
	if (new_index >= max_fragments)
	{
		return;
	}

	uint2 pixel = uint2(input.position.xy);
	uint old_head;
	InterlockedExchange(head_pointer[pixel], new_index, old_head);

	uint2 packed_color = PackColor(final_color);

	OITFragment frag;
	frag.packed_color_rg_ = packed_color.x;
	frag.packed_color_ba_ = packed_color.y;
	frag.depth_ = input.position.z;
	frag.next_ = old_head;
	fragment_buffer[new_index] = frag;
}
