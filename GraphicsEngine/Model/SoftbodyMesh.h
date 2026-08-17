#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>

namespace SeedCore
{
	class BindlessHeap;

	/**
	* [EN]
	* Render-side geometry for a single Softbody actor's deformed mesh.
	* Deliberately bypasses Crister's cluster/LOD geometry-streaming
	* pipeline entirely (a soft body's vertex positions are simulated
	* CPU-side and must land in a stable, page-never-evicted buffer the
	* mesh shader reads). Holds its own small, non-streamed, non-LOD'd
	* CompressedVertex/Meshlet/vertexIndices/primitiveIndices buffer set —
	* built once from the bind-pose full-resolution render mesh
	* (Crister::SoftbodyFinestVertices) — that StaticModelMS.hlsl/
	* MaterialResolveCS.hlsl read exactly like any other Crister-owned
	* instance, because both only ever look at the bindless buffer indices
	* a ModelInstanceData carries, never at Crister itself.
	*
	* Physics does NOT simulate this full-resolution mesh (a mesh
	* shader-scale vertex/edge count is far past what Jolt's soft body
	* solver is meant for — see PhysicsSystem::ResolveSoftbodies). Instead
	* it simulates a much smaller proxy mesh (Crister::SoftbodyCoarsestVertices
	* — each SubMesh's coarsest cluster). Create() binds every
	* full-resolution render vertex to its bindMaxWeights_ nearest proxy
	* vertices (by bind-pose distance, inverse-square-distance weighted),
	* and every Update() blends the proxy's simulated displacement
	* (simulated position - proxy bind pose) through that binding onto the
	* render mesh's bind pose before re-quantising — a simple "cage
	* deformation" transfer (position-delta blending, not rotation-aware;
	* fine for the bend/stretch-dominated look of cloth/jelly, not exact
	* under fast twisting).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Softbody アクター1体ぶんの、変形メッシュのレンダー側ジオメトリ。
	* Crister のクラスタ/LOD ジオメトリストリーミングパイプラインを意図的に
	* 完全にバイパスする（ソフトボディの頂点位置は CPU 側でシミュレートされ、
	* メッシュシェーダが読む「ページが追い出されない安定したバッファ」に
	* 収まる必要がある）。自前の小さな、非ストリーミング・非LODの
	* CompressedVertex/Meshlet/vertexIndices/primitiveIndices バッファ一式を
	* 持つ — バインドポーズのフル解像度描画メッシュ
	* （Crister::SoftbodyFinestVertices）から一度だけ構築する。これは
	* StaticModelMS.hlsl/MaterialResolveCS.hlsl から見れば他の
	* Crister 所有インスタンスと全く同じに読める — どちらも
	* ModelInstanceData が持つ bindless バッファインデックスしか見ておらず、
	* Crister 自体は一切参照しないため。
	*
	* Physics はこのフル解像度メッシュを直接シミュレートしない
	* （メッシュシェーダ規模の頂点/辺数は Jolt のソフトボディソルバーが
	* 想定する規模をはるかに超える — PhysicsSystem::ResolveSoftbodies
	* 参照）。代わりにずっと小さいプロキシメッシュ
	* （Crister::SoftbodyCoarsestVertices — 各 SubMesh の最粗クラスタ）を
	* シミュレートする。Create() が全てのフル解像度描画頂点を、最も近い
	* bindMaxWeights_ 個のプロキシ頂点へ（バインドポーズ距離の逆二乗で
	* 重み付けして）束縛し、毎 Update() がプロキシのシミュレート済み変位
	* （シミュレート位置 - プロキシのバインドポーズ）をその束縛経由で
	* 描画メッシュのバインドポーズへブレンドしてから再量子化する —
	* 単純な「ケージ変形」転送（位置差分ブレンドで回転は考慮しない。
	* 布・ゼリーのような曲げ/伸縮主体の見た目には十分だが、高速なねじれ
	* には正確ではない）。
	*/
	class SoftbodyMesh :public NonCopyable
	{
	public:
		/// [EN] How many nearest proxy vertices each render vertex binds to.
		/// [JP] 各描画頂点が束縛する、最も近いプロキシ頂点の個数。
		static constexpr Uint32 bindMaxWeights_ = 4;

		SoftbodyMesh() = default;
		~SoftbodyMesh() = default;

		/**
		* [EN]
		* Builds the (unchanging) render-mesh meshlet topology from the
		* Crister's full-resolution bind-pose vertices/indices
		* (Crister::SoftbodyFinestVertices), separately reads its simulation
		* proxy's bind pose (Crister::SoftbodyCoarsestVertices) and binds every
		* render vertex to its nearest proxy vertices, then allocates this
		* instance's frame-ring GPU buffers. Returns false if the Crister
		* has no extractable render or proxy geometry.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Crister のフル解像度バインドポーズ頂点/インデックス
		* （Crister::SoftbodyFinestVertices）から (不変の)メシュレットトポロジーを
		* 構築し、別途シミュレーション用プロキシのバインドポーズ
		* （Crister::SoftbodyCoarsestVertices）を読んで全描画頂点を最も近い
		* プロキシ頂点へ束縛した上で、このインスタンスのフレームリング
		* GPU バッファを確保する。Crister に抽出可能な描画/プロキシ
		* ジオメトリが無ければ false を返す。
		*/
		Bool Create(ID3D12Device* device, BindlessHeap* bindlessHeap, const Crister& crister);

		/**
		* [EN]
		* Blends this frame's simulated proxy vertex displacement (against
		* its own bind pose) onto the full-resolution render mesh's bind
		* pose through the Create()-time binding, then re-quantises the
		* result (combined with the cached bind-pose normal/tangent/
		* texcoord) into the vertex buffer against a freshly computed
		* position AABB, and re-uploads the topology buffers into this
		* frame's ring slot. Does nothing if simulatedProxyPositions' size
		* does not match the proxy bind-pose vertex count (Softbody not yet
		* built, or a mismatched Crister).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このフレームのシミュレート済みプロキシ頂点変位（自身のバインド
		* ポーズとの差分）を、Create() 時に構築した束縛経由でフル解像度
		* 描画メッシュのバインドポーズへブレンドし、その結果（キャッシュ
		* 済みのバインドポーズ法線/接線/UV と合成）を、その都度計算した
		* 位置 AABB に対して頂点バッファへ再量子化する。トポロジー
		* バッファもこのフレームのリングスロットへ再アップロードする。
		* simulatedProxyPositions のサイズがプロキシのバインドポーズ頂点数と
		* 一致しなければ何もしない（Softbody がまだ構築されていない、
		* または Crister が不一致）。
		*/
		void Update(const DynamicArray<Vector3>& simulatedProxyPositions);

		[[nodiscard]] Uint VertexBufferIndex()const;
		[[nodiscard]] Uint MeshletBufferIndex()const;
		[[nodiscard]] Uint MeshletBoundBufferIndex()const;
		[[nodiscard]] Uint VertexIndicesBufferIndex()const;
		[[nodiscard]] Uint PrimitiveIndicesBufferIndex()const;

		[[nodiscard]] Uint32 MeshletCount()const;
		[[nodiscard]] Vector3 PositionMin()const;
		[[nodiscard]] Vector3 PositionExtent()const;
		[[nodiscard]] Vector2 TexcoordMin()const;
		[[nodiscard]] Vector2 TexcoordExtent()const;

	private:
		/// [EN] One render vertex's binding to bindMaxWeights_ proxy
		///      vertices (indices into proxyBindPoseVertices_) and their
		///      normalised blend weights.
		/// [JP] 1描画頂点の、bindMaxWeights_ 個のプロキシ頂点
		///      （proxyBindPoseVertices_ へのインデックス）への束縛と、
		///      その正規化済みブレンド重み。
		struct VertexBinding
		{
			Uint32 proxyIndex_[bindMaxWeights_] = {};
			Float weight_[bindMaxWeights_] = {};
		};

		/// [EN] For each renderVertices entry, finds its bindMaxWeights_
		///      nearest proxyVertices entries (by bind-pose distance) via
		///      brute force and weights them by inverse squared distance,
		///      normalised to sum to 1. See Create()'s call site for why
		///      this runs once at build time, not per frame.
		/// [JP] renderVertices の各要素について、最も近い proxyVertices
		///      要素（バインドポーズ距離）を bindMaxWeights_ 個、総当たりで
		///      見つけ、距離の逆二乗で重み付けして合計が1になるよう正規化
		///      する。ビルド時に一度だけ実行する理由は Create() の呼び出し
		///      箇所を参照。
		static void BindRenderVerticesToProxy(const DynamicArray<Vertex>& renderVertices, const DynamicArray<Vertex>& proxyVertices, DynamicArray<VertexBinding>& outBindings);

		/// [EN] Full-resolution render-mesh bind pose (normal_/tangent_/
		///      texcoord_ reused verbatim every frame; position_ is the
		///      base every Update() blends the bound proxy displacement
		///      onto before re-quantising).
		/// [JP] フル解像度描画メッシュのバインドポーズ（normal_/tangent_/
		///      texcoord_ は毎フレームそのまま使い回す。position_ は毎
		///      Update() で束縛したプロキシ変位をブレンドしてから
		///      再量子化する基準値）。
		DynamicArray<Vertex> bindPoseVertices_;

		/// [EN] Simulation proxy's bind pose (Crister::SoftbodyCoarsestVertices)
		///      — only position_ is used, as the reference each Update()
		///      subtracts the frame's simulated proxy position from to get
		///      per-proxy-vertex displacement.
		/// [JP] シミュレーション用プロキシのバインドポーズ
		///      （Crister::SoftbodyCoarsestVertices）— position_ のみ使用する。
		///      毎 Update() でその頂点のシミュレート済み位置から差し引いて
		///      プロキシ頂点ごとの変位を求める基準値。
		DynamicArray<Vertex> proxyBindPoseVertices_;

		/// [EN] Per render vertex (1:1 with bindPoseVertices_), its binding
		///      to the nearest proxy vertices.
		/// [JP] 描画頂点ごと（bindPoseVertices_ と1:1）の、最も近いプロキシ
		///      頂点への束縛。
		DynamicArray<VertexBinding> renderVertexBindings_;

		DynamicArray<Meshlet> meshlets_;
		DynamicArray<Uint32> vertexIndices_;

		/// [EN] Packed 3 bytes per triangle corner, padded to a 4-byte
		///      multiple (ReadOnlyByteAddressBuffer requirement).
		/// [JP] 三角形の頂点1つあたり3バイトに詰めたもの。4バイト境界へ
		///      パディング済み（ReadOnlyByteAddressBuffer の要件）。
		DynamicArray<Uint8> primitiveIndices_;

		Vector2 texcoordMin_ = { 0.0f, 0.0f };
		Vector2 texcoordExtent_ = { 1.0f, 1.0f };

		Vector3 positionMin_ = { 0.0f, 0.0f, 0.0f };
		Vector3 positionExtent_ = { 1.0f, 1.0f, 1.0f };

		/// [EN] Reused across Update() calls to avoid a per-frame allocation.
		/// [JP] 毎フレームの確保を避けるため Update() 間で使い回す。
		DynamicArray<Vector3> scratchProxyDisplacements_;
		DynamicArray<Vector3> scratchDeformedPositions_;
		DynamicArray<CompressedVertex> scratchVertices_;
		DynamicArray<MeshletBound> scratchBounds_;

		ResourcePtr<ReadOnlyStructuredBuffer<CompressedVertex>> vertexBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<Meshlet>> meshletBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<MeshletBound>> meshletBoundBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<Uint32>> vertexIndicesBuffer_;
		ResourcePtr<ReadOnlyByteAddressBuffer> primitiveIndicesBuffer_;
	};
}
