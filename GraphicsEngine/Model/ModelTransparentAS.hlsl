#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared ModelASPayload payload;

/**
* [EN]
* Amplification Shader for the transparent (OIT) pass.
* Full copy of ModelAS.hlsl except that it processes only blend instances
* (instance.blend_ == 1). Keep both files in sync when editing.
*
* Every pass dispatches the full instance list; instances belonging to the
* opaque pass are skipped here. This keeps a single shared instance buffer
* without needing per-dispatch offsets.
*
* ---------------------------------------------------------------------
*
* [JP]
* 透過（OIT）パス用の Amplification Shader。
* ブレンドインスタンス（instance.blend_ == 1）のみ処理する点を除いて
* ModelAS.hlsl の完全なコピー。編集時は両ファイルの同期を保つこと。
*
* 各パスは全インスタンスをディスパッチし、不透明パスに属するインスタンスは
* ここでスキップする。これによりディスパッチごとのオフセットなしで
* 単一の共有インスタンスバッファを使える。
*/
[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID)
{
	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	uint instance_id = gid.x;
	ModelInstance instance = instances[instance_id];
	StructuredBuffer<ModelMeshletBound> bounds = ResourceDescriptorHeap[instance.meshlet_bound_buffer_index_];

	uint meshlet_local = gtid.x;
	bool is_visible = false;

	if (instance.blend_ == 1 && meshlet_local < instance.meshlet_count_)
	{
		/// [EN] Distance-based LOD selection — identical to ModelAS.
		/// [JP] 距離ベース LOD 選択 — ModelAS と同一。
		float world_scale = max(max(length(instance.world_[0].xyz), length(instance.world_[1].xyz)), length(instance.world_[2].xyz));
		if (IsLodSelected(instance.lod_error_, instance.lod_error_next_, instance.world_[3].xyz, world_scale, scene.camera_position_.xyz, scene.projection_._m11, scene.screen_size_.y, 1.0))
		{
		/// [EN] Meshlet bounds are computed from pre-skinning vertex positions.
		///      Skinned geometry can move far away from them, so culling with
		///      these bounds would drop visible meshlets — skip culling instead.
		/// [JP] メシュレットバウンドはスキニング前の頂点位置から計算されている。
		///      スキン後のジオメトリはそこから大きく動きうるため、このバウンドで
		///      カリングすると可視メシュレットが落ちる — カリングをスキップする。
		if (instance.skin_index_ != 0xFFFFFFFF)
		{
			is_visible = true;
		}
		else
		{
		uint meshlet_global = instance.meshlet_offset_ + meshlet_local;
		ModelMeshletBound bound = bounds[meshlet_global];

		/// [EN] Transform bounding sphere center to world space.
		/// [JP] 包囲球の中心をワールド空間に変換する。
		float3 world_center = mul(float4(bound.center_, 1.0), instance.world_).xyz;
		float world_radius = bound.radius_ * max(max(length(instance.world_[0].xyz), length(instance.world_[1].xyz)), length(instance.world_[2].xyz));

		/// [EN] Frustum culling: reject meshlets entirely outside the view frustum.
		/// [JP] フラスタムカリング: ビューフラスタムの完全に外にあるメシュレットを棄却する。
		is_visible = IsVisibleInFrustum(world_center, world_radius, scene.current_view_projection_);

		/// [EN] Normal cone backface culling — single-sided materials only.
		/// [JP] 法線コーンバックフェイスカリング — 片面マテリアルのみ。
		if (is_visible && instance.double_sided_ == 0 && bound.cone_cutoff_ > 0.0)
		{
			float3 world_cone_axis = normalize(mul(float4(bound.cone_axis_, 0.0), instance.world_).xyz);
			float3 view_direction = normalize(scene.camera_position_.xyz - world_center);
			if (dot(view_direction, world_cone_axis) < -bound.cone_cutoff_)
			{
				is_visible = false;
			}
		}

		/// [EN] Hi-Z occlusion culling — the transparent pass runs after the
		///      opaque passes, so the pyramid is already built this frame.
		/// [JP] Hi-Z オクルージョンカリング — 透過パスは不透明パスの後に走るため、
		///      このフレームのピラミッドは構築済み。
		if (is_visible)
		{
			is_visible = IsVisibleHiZ(world_center, world_radius, scene, structured_indices.model_.hi_z_index_);
		}
		}
		}
	}

	if (is_visible)
	{
		uint slot;
		InterlockedAdd(survived_count, 1, slot);
		local_indices[slot] = instance.meshlet_offset_ + meshlet_local;
	}

	GroupMemoryBarrierWithGroupSync();

	if (gtid.x == 0)
	{
		payload.instance_index = instance_id;
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.meshlet_indices[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
