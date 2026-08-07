#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Interop/ColliderInstance.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/Shape/Collider/ColliderLineShader.h>

namespace SeedCore
{
	class ShaderCache;
	class BindlessHeap;
	class D3D12CommandList;

	/// [EN] Per-frame constants for the collider instance shader: which
	///      bindless structured-buffer index holds this frame's collider
	///      instances and how many there are, how many mesh-shader groups
	///      each instance spans (groupsPerInstance_ — needed since a single
	///      64/128-thread group can no longer cover a Jolt-density sphere's
	///      worth of edges), and the bindless index/edge-count of the two
	///      persistent unit-sphere edge tables (full sphere + single
	///      hemisphere, for capsule caps) built once in Create(). Mirrors
	///      ColliderLine.hlsli's ColliderInstanceConstants byte-for-byte.
	/// [JP] コライダーインスタンスシェーダ用の毎フレーム定数: このフレームの
	///      コライダーインスタンスを保持する bindless 構造化バッファの
	///      インデックスと個数、各インスタンスが何個のメッシュシェーダ
	///      グループにまたがるか(groupsPerInstance_ — Jolt本家相当の密度の
	///      球は、もはや1グループ(64/128スレッド)には収まらないため必要)、
	///      Create() で一度だけ構築する単位球エッジテーブル2種（球全体 +
	///      半球1つ、カプセルのキャップ用）の bindless インデックス/辺数。
	///      ColliderLine.hlsli の ColliderInstanceConstants と
	///      バイト単位で一致する。
	struct ColliderInstanceConstants
	{
		Uint lineVertexBufferIndex_ = 0;
		Uint instanceCount_ = 0;
		Uint groupsPerInstance_ = 0;
		Uint sphereEdgeBufferIndex_ = 0;
		Uint sphereEdgeCount_ = 0;
		Uint hemisphereEdgeBufferIndex_ = 0;
		Uint hemisphereEdgeCount_ = 0;
		Uint padding0_ = 0;
	};

	/**
	* [EN]
	* Editor-only debug wireframe renderer for collider visualization.
	* Deliberately does NOT inherit JPH::DebugRenderer — Renderer::Gather
	* walks each Box/Sphere/Capsule/Cylinder/Rect/CircleCollider component in
	* the World directly (regardless of Play/Stop state, since it no longer
	* depends on live JPH::Body instances) and calls AddInstance with each
	* collider's own shape/transform data. The mesh shader then expands each
	* instance's wireframe geometry on the GPU (see ColliderLineMS.hlsl) —
	* this class only uploads the small per-instance descriptor batch and
	* issues one mesh-shader draw per frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コライダー可視化用の、エディタ専用デバッグワイヤーフレームレンダラー。
	* 意図的に JPH::DebugRenderer を継承しない — Renderer::Gather が
	* World 内の各 Box/Sphere/Capsule/Cylinder/Rect/CircleCollider
	* コンポーネントを直接走査し（生きた JPH::Body に依存しなくなったため
	* Play/Stop を問わず動作する）、各コライダー自身の形状/変換データで
	* AddInstance を呼ぶ。ワイヤーフレーム形状の展開はメッシュシェーダが
	* GPU上で行う（ColliderLineMS.hlsl 参照）— このクラスは小さな
	* インスタンス記述子バッチをアップロードし、1フレームにつき1回だけ
	* メッシュシェーダ描画を発行するだけ。
	*/
	class ColliderRenderer
	{
	public:
		/// [EN] Capacity of the collider-instance structured buffer. Instances
		///      beyond this cap within a single frame are silently dropped
		///      rather than reallocating mid-frame.
		/// [JP] コライダーインスタンス構造化バッファの容量。1フレーム内で
		///      これを超えたインスタンスは、フレーム途中の再確保を避けるため
		///      黙って破棄される。
		static constexpr Uint maxInstanceCount_ = 8192;

		/// [EN] Icosahedron subdivision level for the sphere/capsule-cap edge
		///      tables — level 3 matches JPH::DebugRenderer's own default
		///      DrawWireSphere density (20 * 4^3 = 1280 faces, 1920 edges).
		/// [JP] 球/カプセルキャップ用エッジテーブルの正20面体細分割レベル —
		///      レベル3は JPH::DebugRenderer 自身の DrawWireSphere 既定密度
		///      (20 * 4^3 = 1280面、1920辺)と一致する。
		static constexpr Uint icosphereSubdivisionLevel_ = 3;

		/// [EN] Must match ColliderLineMS.hlsl's GetCylinderBodyLine /
		///      GetLineCount hardcoded ring/vertical segment counts exactly —
		///      only used here to size groupsPerInstance_, not to generate
		///      any geometry (cylinder/circle/box/rect stay fully procedural
		///      in the shader).
		/// [JP] ColliderLineMS.hlsl の GetCylinderBodyLine / GetLineCount が
		///      ハードコードしているリング/縦線の分割数と厳密に一致させる
		///      こと — ここでは groupsPerInstance_ のサイズ計算にのみ使う
		///      (cylinder/circle/box/rect の形状生成自体は今も完全に
		///      シェーダ側の手続き生成のまま)。
		static constexpr Uint cylinderRingSegments_ = 32;
		static constexpr Uint cylinderVerticalLineCount_ = 8;

		ColliderRenderer() = default;
		~ColliderRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache);

		/// [EN] Resets the CPU-side instance batch for a new frame. Called by
		///      Renderer::Gather before it repopulates it from the World's
		///      collider components.
		/// [JP] 新しいフレームに向けて CPU 側のインスタンスバッチをリセットする。
		///      Renderer::Gather が World のコライダーコンポーネントから
		///      再び積み込む前に呼ぶ。
		void Clear();

		void AddInstance(ColliderShapeKind shapeKind, const Vector3& position, const Quaternion& rotation, const Vector3& dimensions, const Color& color);

		/// [EN] Uploads this frame's accumulated instance batch and issues the
		///      mesh-shader draw call against the given render target/depth
		///      views. Does nothing if no instances were accumulated this
		///      frame. Takes raw views/viewport rather than FrameBuffer*/
		///      GeometryBuffer* so it can draw onto any target - the editor
		///      frame buffer's color + geometry buffer's depth (as today), or
		///      PostProcessRenderer's post-tonemap output + a matching depth
		///      view (see Renderer::EndEditorFrame). Caller owns every
		///      resource's state transitions.
		/// [JP] このフレームで蓄積したインスタンスバッチをアップロードし、
		///      指定されたレンダーターゲット/深度ビューへメッシュシェーダの
		///      描画コマンドを発行する。このフレームに1つも蓄積されて
		///      いなければ何もしない。FrameBuffer*/GeometryBuffer* ではなく
		///      生のビュー/ビューポートを受け取ることで、どんなターゲットへも
		///      描画できる - エディタフレームバッファの色+ジオメトリバッファの
		///      深度(現状通り)、あるいは PostProcessRenderer のトーンマップ後
		///      出力+対応する深度ビュー(Renderer::EndEditorFrame参照)。
		///      各リソースの状態遷移は呼び出し側の責任。
		void Draw(D3D12CommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView, D3D12_VIEWPORT viewport, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex);

	private:
		/// [EN] Owned directly rather than reusing Renderer's shared
		///      PipelineStateObject cache — ColliderLineShader builds its
		///      own dedicated RootSignature (see its class comment), and
		///      PipelineStateObject::GetOrCreate keys purely off the raw
		///      ID3D12RootSignature pointer passed in, so there is no
		///      benefit to sharing the cache instance.
		/// [JP] Renderer が持つ共有 PipelineStateObject キャッシュを使い回さず
		///      直接所有する — ColliderLineShader は自前の専用 RootSignature
		///      を構築し（クラスコメント参照）、PipelineStateObject::GetOrCreate
		///      は渡された生の ID3D12RootSignature ポインタのみをキーにする
		///      ため、キャッシュインスタンスを共有する利点がない。
		PipelineStateObject pipelineStateObject_;

		ColliderLineShader colliderLineShader_;

		DynamicArray<ColliderInstance> instances_;

		ResourcePtr<ReadOnlyStructuredBuffer<ColliderInstance>> instanceBuffer_;
		ResourcePtr<ConstantBuffer<ColliderInstanceConstants>> instanceConstantsBuffer_;

		/// [EN] Persistent (never change after Create()) unit-sphere edge
		///      tables, built once from a subdivided icosahedron. Re-uploaded
		///      every Draw() anyway — ReadOnlyStructuredBuffer is a
		///      frame-ring upload buffer, so a single upload at Create()
		///      would only populate one of its in-flight slots.
		/// [JP] Create() 以降は不変の単位球エッジテーブル。細分割した
		///      正20面体から一度だけ構築する。Draw() のたびに再アップロード
		///      している理由: ReadOnlyStructuredBuffer はフレームリング式の
		///      アップロードバッファなので、Create() で1回だけアップロード
		///      すると、インフライトスロットの1つにしか書き込まれない。
		DynamicArray<Vector3> sphereEdgeData_;
		DynamicArray<Vector3> hemisphereEdgeData_;

		ResourcePtr<ReadOnlyStructuredBuffer<Vector3>> sphereEdgeBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<Vector3>> hemisphereEdgeBuffer_;

		Uint sphereEdgeCount_ = 0;
		Uint hemisphereEdgeCount_ = 0;

		/// [EN] How many mesh-shader groups a single collider instance spans,
		///      computed once in Create() from the densest shape's line
		///      count (always the sphere/capsule after Jolt-density
		///      subdivision) divided by threadsPerGroup_.
		/// [JP] コライダー1インスタンスが何個のメッシュシェーダグループに
		///      またがるか。Create() で一度だけ、最も線分数の多い形状
		///      (Jolt相当密度に細分割した後は常に球/カプセル)の線数を
		///      threadsPerGroup_ で割って求める。
		static constexpr Uint threadsPerGroup_ = 128;
		Uint groupsPerInstance_ = 1;

		BindlessHeap* bindlessHeap_ = nullptr;
	};
}
