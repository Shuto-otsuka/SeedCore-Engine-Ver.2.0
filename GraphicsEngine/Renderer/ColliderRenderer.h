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
	class ConstantIndicesSystem;

	/// [EN] Per-frame constants for the collider instance shader, one per
	///      batch (3D or 2D): which bindless structured-buffer index holds
	///      that batch's collider instances and how many there are, how many mesh-shader groups
	///      each instance spans (groupsPerInstance_ — needed since a single
	///      64/128-thread group can no longer cover a Jolt-density sphere's
	///      worth of edges), and the bindless index/edge-count of the two
	///      persistent unit-sphere edge tables (full sphere + single
	///      hemisphere, for capsule caps) built once in Create(). Mirrors
	///      ColliderLine.hlsli's ColliderConstantBuffer byte-for-byte.
	/// [JP] コライダーインスタンスシェーダ用の毎フレーム定数で、バッチ（3D か 2D）
	///      ごとに1つずつ持つ: そのバッチのコライダーインスタンスを保持する
	///      bindless 構造化バッファのインデックスと個数、各インスタンスが何個のメッシュシェーダ
	///      グループにまたがるか(groupsPerInstance_ — Jolt本家相当の密度の
	///      球は、もはや1グループ(64/128スレッド)には収まらないため必要)、
	///      Create() で一度だけ構築する単位球エッジテーブル2種（球全体 +
	///      半球1つ、カプセルのキャップ用）の bindless インデックス/辺数。
	///      ColliderLine.hlsli の ColliderConstantBuffer と
	///      バイト単位で一致する。
	struct ColliderConstantBuffer
	{
		Uint instanceBufferIndex_ = 0;
		Uint instanceCount_ = 0;
		Uint groupsPerInstance_ = 0;
		Uint sphereEdgeBufferIndex_ = 0;

		Uint sphereEdgeCount_ = 0;
		Uint hemisphereEdgeBufferIndex_ = 0;
		Uint hemisphereEdgeCount_ = 0;
		Uint colliderConstantBufferPadding0_ = 0;
	};

	/**
	* [EN]
	* Editor-only debug wireframe renderer for collider visualization.
	* Deliberately does NOT inherit JPH::DebugRenderer — Renderer::Gather
	* walks each Box/Sphere/Capsule/Cylinder/Rect/CircleCollider component in
	* the World directly (regardless of Play/Stop state, since it no longer
	* depends on live JPH::Body instances) and calls AddInstance with each
	* collider's own shape/transform data. The mesh shader (or, below D12_2,
	* the vertex shader) then expands each instance's wireframe geometry on
	* the GPU (see ColliderLineMS.hlsl / ColliderLineVS.hlsl) — this class
	* only uploads the small per-instance descriptor batches and issues the
	* draws.
	*
	* Instances are kept in two independent batches: spatial (3D colliders,
	* drawn into the editor's 3D view by Draw3D) and planar (Rect/Circle,
	* drawn onto the canvas by Draw2D). Each batch has its own instance and
	* constant buffer, and its constant index is registered only in the
	* index table of the view that draws it, so each draw reads nothing but
	* its own batch with no shader-side filtering.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コライダー可視化用の、エディタ専用デバッグワイヤーフレームレンダラー。
	* 意図的に JPH::DebugRenderer を継承しない — Renderer::Gather が
	* World 内の各 Box/Sphere/Capsule/Cylinder/Rect/CircleCollider
	* コンポーネントを直接走査し（生きた JPH::Body に依存しなくなったため
	* Play/Stop を問わず動作する）、各コライダー自身の形状/変換データで
	* AddInstance を呼ぶ。ワイヤーフレーム形状の展開はメッシュシェーダ
	* （D12_2 未満では頂点シェーダ）が GPU上で行う（ColliderLineMS.hlsl /
	* ColliderLineVS.hlsl 参照）— このクラスは小さなインスタンス記述子
	* バッチをアップロードし、描画を発行するだけ。
	*
	* インスタンスは独立した2つのバッチに分けて持つ: spatial（3D コライダー。
	* Draw3D がエディタの 3D ビューへ描く）と planar（Rect/Circle。Draw2D が
	* Canvas へ描く）。各バッチは専用のインスタンスバッファと定数バッファを持ち、
	* その定数インデックスは、それを描くビューのインデックステーブルにだけ
	* 登録する。そのため各描画は自分のバッチだけを読み、シェーダ側での
	* 振り分けは要らない。
	*/
	class ColliderRenderer
	{
	public:
		ColliderRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ColliderRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, ConstantIndicesSystem& constantIndicesSystem);

		/// [EN] Resets both CPU-side instance batches (spatial and planar) for
		///      a new frame. Called by Renderer::Gather before it repopulates
		///      them from the World's collider components.
		/// [JP] 新しいフレームに向けて、CPU 側の2つのインスタンスバッチ
		///      （spatial と planar）をリセットする。Renderer::Gather が World の
		///      コライダーコンポーネントから再び積み込む前に呼ぶ。
		void Clear();

		/// [EN] Appends one collider instance to the batch its shape belongs
		///      to: Rect/Circle go to the planar batch, every other shape to
		///      the spatial batch. Instances past maxInstanceCount_ in that
		///      batch are dropped.
		/// [JP] コライダーインスタンスを1つ、その形状が属するバッチへ追加する:
		///      Rect/Circle は planar バッチ、それ以外の形状はすべて spatial
		///      バッチ。そのバッチで maxInstanceCount_ を超えた分は破棄する。
		void AddInstance(ColliderShapeKind shapeKind, const Vector3& position, const Quaternion& rotation, const Vector3& dimensions, const Color& color);

		/// [EN] Uploads each non-empty batch to its own instance and constant
		///      buffer, then registers the spatial constants in the editor's
		///      index table and the planar constants in the canvas's.
		/// [JP] 空でない各バッチを専用のインスタンスバッファ/定数バッファへ
		///      アップロードし、spatial の定数をエディタのインデックステーブルへ、
		///      planar の定数を Canvas のインデックステーブルへ登録する。
		void Upload();

		/// [EN] Draws the spatial (3D) batch Upload() sent this frame into the
		///      editor's 3D view, depth-tested against the given depth view.
		///      Must be called with the editor's root addresses, since the
		///      spatial constants are registered only in the editor's index
		///      table. Does nothing if the spatial batch is empty. Takes raw
		///      views/viewport rather than FrameBuffer*/GeometryBuffer* so the
		///      caller picks the target - PostProcessRenderer's post-tonemap
		///      output + a matching depth view (see Renderer::EndEditorFrame).
		///      Caller owns every resource's state transitions.
		/// [JP] このフレームに Upload() が送った spatial（3D）バッチを、エディタの
		///      3D ビューへ、指定された深度ビューで深度テストしながら描く。
		///      spatial の定数はエディタのインデックステーブルにだけ登録されるので、
		///      エディタのルートアドレスで呼ぶこと。spatial バッチが空なら何もしない。
		///      FrameBuffer*/GeometryBuffer* ではなく生のビュー/ビューポートを
		///      受け取るので、描画先は呼び出し側が決める - PostProcessRenderer の
		///      トーンマップ後出力 + 対応する深度ビュー(Renderer::EndEditorFrame参照)。
		///      各リソースの状態遷移は呼び出し側の責任。
		void Draw3D(D3D12CommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView, D3D12_VIEWPORT viewport, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		/// [EN] Draws the planar (2D) batch Upload() sent this frame onto the
		///      canvas. Must be called with the canvas's root addresses, since
		///      the planar constants are registered only in the canvas's index
		///      table. Binds no depth view and draws with depth testing off:
		///      canvas sprites lie on the same plane as the collider outlines,
		///      so a depth test would hide the outlines wherever a sprite is.
		///      Does nothing if the planar batch is empty.
		/// [JP] このフレームに Upload() が送った planar（2D）バッチを Canvas へ描く。
		///      planar の定数は Canvas のインデックステーブルにだけ登録されるので、
		///      Canvas のルートアドレスで呼ぶこと。深度ビューは bind せず、
		///      深度テストなしで描く: Canvas のスプライトはコライダーの輪郭と
		///      同じ平面にあるため、深度テストをするとスプライトのある場所で
		///      輪郭が隠れてしまう。planar バッチが空なら何もしない。
		void Draw2D(D3D12CommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_VIEWPORT viewport, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

	private:
		/// [EN] Capacity of each batch's collider-instance structured buffer
		///      (spatial and planar each get this many). Instances beyond this
		///      cap within a single frame are silently dropped rather than
		///      reallocating mid-frame.
		/// [JP] 各バッチのコライダーインスタンス構造化バッファの容量（spatial と
		///      planar がそれぞれこの数を持つ）。1フレーム内でこれを超えた
		///      インスタンスは、フレーム途中の再確保を避けるため黙って破棄される。
		static constexpr Uint maxInstanceCount_ = 8192;

		/// [EN] Icosahedron subdivision level for the sphere/capsule-cap edge
		///      tables — level 3 matches JPH::DebugRenderer's own default
		///      DrawWireSphere density (20 * 4^3 = 1280 faces, 1920 edges).
		/// [JP] 球/カプセルキャップ用エッジテーブルの正20面体細分割レベル —
		///      レベル3は JPH::DebugRenderer 自身の DrawWireSphere 既定密度
		///      (20 * 4^3 = 1280面、1920辺)と一致する。
		static constexpr Uint icosphereSubdivisionLevel_ = 3;

		/// [EN] Must match the ring/vertical segment counts hardcoded in
		///      ColliderLine.hlsli's GetCylinderLine and the line counts in
		///      ColliderLineMS.hlsl / ColliderLineVS.hlsl exactly — only used
		///      here to size groupsPerInstance_, not to generate any geometry
		///      (cylinder/circle/box/rect stay fully procedural in the shader).
		/// [JP] ColliderLine.hlsli の GetCylinderLine と ColliderLineMS.hlsl /
		///      ColliderLineVS.hlsl の線数がハードコードしているリング/縦線の
		///      分割数と厳密に一致させること — ここでは groupsPerInstance_ のサイズ計算にのみ使う
		///      (cylinder/circle/box/rect の形状生成自体は今も完全に
		///      シェーダ側の手続き生成のまま)。
		static constexpr Uint cylinderRingSegments_ = 32;
		static constexpr Uint cylinderVerticalLineCount_ = 8;

		ColliderLineShader colliderLineShader_;

		/// [EN] CPU-side instance batches for the current frame: spatial holds the 3D colliders, planar holds Rect/Circle.
		/// [JP] 現フレームの CPU 側インスタンスバッチ: spatial は 3D コライダー、planar は Rect/Circle を保持する。
		DynamicArray<ColliderStructuredBuffer> spatialInstances_;
		DynamicArray<ColliderStructuredBuffer> planarInstances_;

		/// [EN] GPU copies of the two batches, read by the shader through the constant buffer of the same batch.
		/// [JP] 2つのバッチの GPU 側コピー。シェーダは同じバッチの定数バッファを経由して読む。
		ResourcePtr<ReadOnlyStructuredBuffer<ColliderStructuredBuffer>> spatialInstanceBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<ColliderStructuredBuffer>> planarInstanceBuffer_;

		/// [EN] Per-batch constants. The spatial one is registered in the editor's index table, the planar one in the canvas's.
		/// [JP] バッチごとの定数。spatial はエディタの、planar は Canvas のインデックステーブルに登録する。
		ResourcePtr<ConstantBuffer<ColliderConstantBuffer>> spatialInstanceConstantsBuffer_;
		ResourcePtr<ConstantBuffer<ColliderConstantBuffer>> planarInstanceConstantsBuffer_;

		/// [EN] Persistent (never change after Create()) unit-sphere edge
		///      tables, built once from a subdivided icosahedron. Re-uploaded
		///      every Upload() anyway — ReadOnlyStructuredBuffer is a
		///      frame-ring upload buffer, so a single upload at Create()
		///      would only populate one of its in-flight slots.
		/// [JP] Create() 以降は不変の単位球エッジテーブル。細分割した
		///      正20面体から一度だけ構築する。Upload() のたびに再アップロード
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
		ConstantIndicesSystem* constantIndicesSystem_ = nullptr;
	};
}
