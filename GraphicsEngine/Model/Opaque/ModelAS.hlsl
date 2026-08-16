#include "../Model.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared ModelASPayload payload;

/**
* [EN]
* Reference: https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html
*
* Amplification Shader for model rendering (shared by Static & Skeletal).
*
* Each thread group processes one ModelInstance. Each thread within the
* group evaluates one meshlet for visibility:
*   1. Frustum culling via bounding sphere
*   2. Normal cone backface culling
*
* Surviving meshlets are compacted into the payload and dispatched
* to the Mesh Shader via DispatchMesh.
*
* Thread group size = 32, matching the maximum meshlets per dispatch.
*
* ---------------------------------------------------------------------
*
* [JP]
* モデルレンダリング用の Amplification Shader（Static と Skeletal で共有）。
*
* 各スレッドグループが 1 つの ModelInstance を処理する。グループ内の
* 各スレッドが 1 つのメシュレットの可視性を評価する:
*   1. 包囲球によるフラスタムカリング
*   2. 法線コーンによるバックフェイスカリング
*
* 生き残ったメシュレットはペイロードに圧縮され、DispatchMesh で
* Mesh Shader に送られる。
*
* スレッドグループサイズ = 32、ディスパッチあたりの最大メシュレット数に一致。
*/
/// [EN] This entry handles the opaque + mask passes (G-Buffer / depth prepass)
///      and skips blend instances — those are drawn by ModelTransparentAS.hlsl
///      (a full copy of this shader; keep both in sync). Every pass dispatches
///      the full instance list, so no per-dispatch offset is needed.
/// [JP] このエントリは不透明 + マスクパス（G-Buffer / デプスプリパス）を担当し、
///      ブレンドインスタンスはスキップする — それらは ModelTransparentAS.hlsl
///      （本シェーダの完全コピー。編集時は同期を保つこと）が描画する。
///      各パスは全インスタンスをディスパッチするため、ディスパッチごとの
///      オフセットは不要。
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

	if (instance.blend_ == 0 && meshlet_local < instance.meshlet_count_)
	{
		/// [EN] Distance-based LOD selection: only the cluster whose cumulative
		///      screen-space error brackets the threshold survives. Must match the
		///      G-Buffer AS exactly so prepass depth and G-Buffer geometry agree.
		/// [JP] 距離ベース LOD 選択: 累積スクリーン誤差が閾値を挟むクラスタだけが
		///      生き残る。プリパス深度と G-Buffer ジオメトリを一致させるため、
		///      G-Buffer 用 AS と完全に同じ判定であること。
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
		///      doubleSided materials must keep back-facing meshlets, otherwise thin
		///      geometry (wings, hair, cloth) gets holes when seen from behind.
		/// [JP] 法線コーンバックフェイスカリング — 片面マテリアルのみ。
		///      doubleSided マテリアルは背面向きメシュレットも残す必要がある。
		///      さもないと薄いジオメトリ（翼・髪・布）を裏から見たときに穴が開く。
		if (is_visible && instance.double_sided_ == 0 && bound.cone_cutoff_ > 0.0)
		{
			float3 world_cone_axis = normalize(mul(float4(bound.cone_axis_, 0.0), instance.world_).xyz);
			float3 view_direction = normalize(scene.camera_position_.xyz - world_center);
			if (dot(view_direction, world_cone_axis) < -bound.cone_cutoff_)
			{
				is_visible = false;
			}
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
