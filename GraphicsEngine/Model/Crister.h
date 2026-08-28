#pragma once
#include <FoundationEngine/Prelude.h>

/**
* [EN]
* Texture mip streaming: implemented alongside geometry StreamingGeometry
* streaming, same shape (coarsest pinned resident, finer levels on demand,
* EvictClusterBudget/EvictTextureBudget reclaim under separate VRAM
* budgets — see MakeClusterResident/EvictCluster and
* MakeTextureMipResident/EvictTextureMip). Picked option A from the two
* considered: one committed resource PER MIP LEVEL, created/freed
* independently (mirrors StreamingGeometry's per-cluster buffers), over
* one MipLevels=N resource with only some subresources uploaded
* (rejected — a DEFAULT-heap resource reserves memory for its full mip
* chain at creation regardless of which subresources hold data, so that
* option would not have reduced VRAM usage). No Model.hlsli change was
* needed: UV space is unaffected by which single-mip resource currently
* backs the SRV, so the existing sampling code works unmodified against
* whatever resolution happens to be resident.
*
* ---------------------------------------------------------------------
*
* [JP]
* テクスチャミップストリーミング: ジオメトリの StreamingGeometry ストリーミングと
* 同じ形で実装済み（最粗ミップをピン留めして常駐、細かいミップはオンデマンド、
* EvictClusterBudget/EvictTextureBudget がそれぞれ別の VRAM 予算超過時に回収 —
* MakeClusterResident/EvictCluster と MakeTextureMipResident/EvictTextureMip
* 参照）。検討した2案のうちA案を採用: ミップレベルごとに別々の committed
* resource を作り個別に作成/破棄する方式（StreamingGeometry のクラスタごとバッファと
* 同じ発想）。MipLevels=N の1リソースを作り一部のサブリソースにしかデータを
* 入れない方式は却下 — DEFAULT ヒープのリソースはサブリソースにデータが
* 入っているかに関わらず作成時点でフルミップチェーン分のメモリを確保するため、
* VRAM 削減にならなかったはず。Model.hlsli の変更は不要だった: どの単一ミップの
* リソースが SRV の裏にあっても UV 空間は変わらないため、既存のサンプリング
* コードは常駐中の解像度に対してそのまま動作する。
*/
namespace SeedCore
{
	class BindlessHeap;
	class BC7CompressShader;
	class D3D12CommandQueue;

	/**
	* [EN]
	* Full-precision CPU-side working vertex format, populated by
	* ModelLoader::FetchMeshes straight from glTF accessors. Only alive
	* during the load/bake pipeline — BakeMesh() quantises this into
	* CompressedVertex/CompressedSkinVertex and discards it, so it is never
	* itself serialised to the .crister cache.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フル精度の CPU 側ワーキング頂点フォーマット。ModelLoader::FetchMeshes
	* が glTF アクセサから直接埋める。ロード/ベイクのパイプライン中のみ生存
	* — BakeMesh() がこれを CompressedVertex/CompressedSkinVertex へ量子化
	* して破棄するため、これ自体が .crister キャッシュへシリアライズされる
	* ことは無い。
	*/
	struct Vertex
	{
		Vector3 position_ = { 0,0,0 };
		Vector3 normal_ = { 0,0,1 };
		Vector4 tangent_ = { 1,0,0,1 };
		Vector2 texcoord_ = { 0,0 };
		XmUint4 joints_ = { 0,0,0,0 };
		Vector4 weights_ = { 1,0,0,0 };
	};

	/**
	* [EN]
	* GPU vertex format, quantised on the CPU at BakeMesh() time (load/bake,
	* baked into the .crister cache) and decoded in the mesh/hit shaders
	* (Model.hlsli: DecodeModelVertex). 16 bytes vs the 80-byte source
	* Vertex. Positions/texcoords are 16-bit UNORM against the whole
	* Crister's AABB (shared vertices quantise identically, so meshlet
	* seams cannot crack); normals are 16+16-bit octahedral; tangents are
	* 8+7-bit octahedral plus a handedness sign.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GPU 頂点フォーマット。BakeMesh() 時（ロード/ベイク時、.crister キャッシュに
	* 焼き込み）に CPU で量子化し、メッシュ/ヒットシェーダでデコードする
	* (Model.hlsli: DecodeModelVertex)。元 Vertex の 80 バイトに対し 16 バイト。
	* 位置/UV は Crister 全体の AABB に対する 16bit UNORM（共有頂点は同一に
	* 量子化されるため、メシュレット境界で亀裂は出ない）。法線は 16+16bit
	* octahedral、タンジェントは 8+7bit octahedral + 利き手符号 1bit。
	*/
	struct CompressedVertex
	{
		Uint32 positionXY_ = 0;    // x:16 | y:16 (UNORM in AABB)
		Uint32 positionZTexU_ = 0; // z:16 | texcoord u:16 (UNORM in UV AABB)
		Uint32 texVTangent_ = 0;   // texcoord v:16 | tangent oct x:8 y:7 sign:1
		Uint32 normal_ = 0;        // octahedral x:16 | y:16

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("position_xy", positionXY_);
			archive.Field("position_z_tex_u", positionZTexU_);
			archive.Field("tex_v_tangent", texVTangent_);
			archive.Field("normal", normal_);
		}
	};

	/**
	* [EN]
	* Skinning attributes, split from CompressedVertex so static models
	* never pay for them. Uploaded only when the Crister has skins.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スキニング属性。静的モデルがコストを払わずに済むよう
	* CompressedVertex から分離。スキンを持つ Crister のみアップロード。
	*/
	struct CompressedSkinVertex
	{
		Uint32 jointsXY_ = 0; // joint0:16 | joint1:16
		Uint32 jointsZW_ = 0; // joint2:16 | joint3:16
		Uint32 weights_ = 0;  // 4 x 8-bit UNORM, renormalised in the shader

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("joints_xy", jointsXY_);
			archive.Field("joints_zw", jointsZW_);
			archive.Field("weights", weights_);
		}
	};

	/**
	* [EN]
	* GPU meshlet-shader input: one meshlet's vertex/triangle range within
	* its owning Cluster's page, addressed relative to vertexIndices_/
	* primitiveIndices_ (page-local once a page is uploaded — see
	* StreamingGeometry). A Cluster spans meshletOffset_/meshletCount_ of
	* these.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GPU メッシュシェーダ入力: 1 meshlet ぶんの、所属 Cluster のページ内での
	* 頂点/三角形範囲。vertexIndices_/primitiveIndices_ を基準に指す
	* （ページがアップロードされた後はページローカルになる — StreamingGeometry
	* 参照）。Cluster は meshletOffset_/meshletCount_ 個ぶんのこれをまとめる。
	*/
	struct Meshlet
	{
		Uint32 vertexOffset_ = 0;
		Uint32 triangleOffset_ = 0;
		Uint32 vertexCount_ = 0;
		Uint32 triangleCount_ = 0;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("vertex_offset", vertexOffset_);
			archive.Field("triangle_offset", triangleOffset_);
			archive.Field("vertex_count", vertexCount_);
			archive.Field("triangle_count", triangleCount_);
		}
	};

	/**
	* [EN]
	* Culling bound for one Meshlet: a bounding sphere (center_/radius_)
	* plus a normal cone (coneAxis_/coneCutoff_) for backface-cluster
	* culling. One entry per Meshlet, same indexing as meshlets_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Meshlet 1 つぶんのカリング用バウンド: バウンディングスフィア
	* (center_/radius_) と、背面クラスタカリング用の法線コーン
	* (coneAxis_/coneCutoff_)。meshlets_ と同じインデックスで 1 対 1。
	*/
	struct MeshletBound
	{
		Vector3 center_ = { 0,0,0 };
		Float radius_ = 0.0f;
		Vector3 coneAxis_ = { 0,0,1 };
		Float coneCutoff_ = 1.0f;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("center", center_);
			archive.Field("radius", radius_);
			archive.Field("cone_axis", coneAxis_);
			archive.Field("cone_cutoff", coneCutoff_);
		}
	};

	/**
	* [EN]
	* One LOD level's meshlet range within a SubMesh: meshletOffset_/
	* meshletCount_ index into meshlets_/meshletBounds_, lodLevel_ selects
	* the detail tier (0 = most detailed), and lodError_ is the QEM
	* simplification error used to pick which Cluster to draw for a given
	* screen coverage. This is also the streaming granularity — see
	* StreamingGeometry, MakeClusterResident/EvictCluster.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* SubMesh 内の 1 LOD レベルぶんの meshlet 範囲: meshletOffset_/
	* meshletCount_ で meshlets_/meshletBounds_ を指す。lodLevel_ が
	* 詳細度の段（0 が最も詳細）、lodError_ は QEM 簡略化誤差で、画面
	* 被覆率に応じてどの Cluster を描画するか選ぶのに使う。これは
	* ストリーミングの粒度でもある — StreamingGeometry、
	* MakeClusterResident/EvictCluster 参照。
	*/
	struct Cluster
	{
		Uint32 meshletOffset_ = 0;
		Uint32 meshletCount_ = 0;
		Uint32 lodLevel_ = 0;
		Float lodError_ = 0.0f;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("meshlet_offset", meshletOffset_);
			archive.Field("meshlet_count", meshletCount_);
			archive.Field("lod_level", lodLevel_);
			archive.Field("lod_error", lodError_);
		}
	};

	/**
	* [EN]
	* One glTF morph target belonging to a SubMesh: a display name plus
	* per-vertex position deltas, one entry per vertex in that SubMesh (index
	* i is the delta for the vertex at crister.vertices_[vertexOffset + i]
	* during load, and stays index-aligned with the SubMesh's own vertex
	* range afterwards). Kept as full-precision Vector3 rather than folded
	* into CompressedVertex's shared-AABB quantisation: a morph delta's
	* magnitude is unrelated to the mesh's own bounds and quantising it
	* against them would misrepresent targets that move vertices far
	* outside the rest pose.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* SubMesh に属する glTF モーフターゲット1つぶん: 表示名と、頂点ごとの
	* 位置デルタ（そのSubMeshの頂点1つにつき1エントリ。ロード時はインデックス i が
	* crister.vertices_[vertexOffset + i] のデルタに対応し、その後もSubMesh自身の
	* 頂点範囲とインデックスが揃ったまま保たれる）。CompressedVertex の
	* 共有AABB量子化には畳み込まずフル精度のVector3で保持する — モーフデルタの
	* 大きさはメッシュ自身の境界とは無関係で、それに対して量子化すると
	* レストポーズから大きく外れて動くターゲットを正しく表現できない。
	*/
	struct Morph
	{
		std::string name_;
		DynamicArray<Vector3> positionDeltas_;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("name", name_);
			archive.Field("position_deltas", positionDeltas_);
		}
	};

	/**
	* [EN]
	* One drawable piece of the model: a single material assignment plus
	* clusterOffset_/clusterCount_ (all its LOD Clusters) and
	* indexOffset_/indexCount_ (its range in the flat 32-bit triangle
	* index buffer, for RT). Every glTF primitive becomes one SubMesh.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* モデルの描画可能な1パーツ: 単一のマテリアル割り当てと
	* clusterOffset_/clusterCount_（全 LOD の Cluster）、
	* indexOffset_/indexCount_（フラット 32bit 三角形インデックスバッファ
	* 内の範囲、RT 用）。glTF の各プリミティブが1つの SubMesh になる。
	*/
	struct SubMesh
	{
		Uint32 clusterOffset_ = 0;
		Uint32 clusterCount_ = 0;
		Uint32 materialIndex_ = 0;
		Uint32 indexOffset_ = 0;
		Uint32 indexCount_ = 0;

		/// [EN] Index into Crister::skins_ if this SubMesh is skinned, -1 otherwise.
		///      Resolved from the glTF node that references this SubMesh's mesh.
		/// [JP] この SubMesh がスキンドなら Crister::skins_ へのインデックス、それ以外は -1。
		///      この SubMesh のメッシュを参照する glTF ノードから解決される。
		Int skinIndex_ = -1;

		/// [EN] Index into the source glTF model's meshes array this SubMesh
		///      came from (Node::mesh_ matches this same index space). Since
		///      FetchMeshes iterates gltfMesh then its primitives in order,
		///      every SubMesh sharing a meshIndex_ is a contiguous run in
		///      Crister::subMeshes_. Used to route a Node's animated morph
		///      weights (Animation::weights_, keyed by target node index) to
		///      the SubMeshes that node's mesh produced.
		/// [JP] この SubMesh の出所である、元 glTF モデルの meshes 配列への
		///      インデックス(Node::mesh_ と同じインデックス空間)。
		///      FetchMeshes は gltfMesh とそのプリミティブを順番に走査する
		///      ため、同じ meshIndex_ を持つ SubMesh は Crister::subMeshes_
		///      内で連続する。ノードのアニメーションモーフウェイト
		///      (Animation::weights_、対象ノード番号がキー)を、その
		///      ノードのメッシュが生成した SubMesh 群へ振り分けるのに使う。
		Int meshIndex_ = -1;

		/// [EN] Empty for a SubMesh with no glTF morph targets.
		/// [JP] glTF モーフターゲットを持たない SubMesh では空。
		DynamicArray<Morph> morphs_;

		/// [EN] This SubMesh's vertex range in Crister::vertices_ as of
		///      FetchMeshes — [vertexOffset_, vertexOffset_ + vertexCount_).
		///      LOD-duplicated copies BuildMeshlets appends afterwards fall
		///      outside this range. Exists solely to resolve a global vertex
		///      index back to a local index into Morph::positionDeltas_ (see
		///      Morph's own comment) when baking the RT morph delta buffer.
		/// [JP] FetchMeshes 時点での、この SubMesh の Crister::vertices_
		///      における頂点範囲 — [vertexOffset_, vertexOffset_ +
		///      vertexCount_)。BuildMeshlets が後から追加する LOD 複製
		///      コピーはこの範囲外。RT 用モーフデルタバッファを焼き込む際、
		///      グローバル頂点インデックスを Morph::positionDeltas_ への
		///      ローカルインデックスへ逆引きする(Morph 自身のコメント参照)
		///      用途のみに存在する。
		Uint32 vertexOffset_ = 0;
		Uint32 vertexCount_ = 0;

		/// [EN] RT proxy's compact vertex range for this SubMesh —
		///      [raytracingVertexOffset_, raytracingVertexOffset_ + raytracingVertexCount_) into
		///      Crister::positionResource_/raytracingMorphDeltaResource_. Runtime-only,
		///      rebuilt every load by Crister::Upload's RT proxy pass — not
		///      serialized.
		/// [JP] この SubMesh の RT プロキシにおけるコンパクト頂点範囲 —
		///      Crister::positionResource_/raytracingMorphDeltaResource_ 内の
		///      [raytracingVertexOffset_, raytracingVertexOffset_ + raytracingVertexCount_)。実行時
		///      のみで、Crister::Upload の RT プロキシ構築パスで毎回再構築
		///      される — シリアライズしない。
		Uint32 raytracingVertexOffset_ = 0;
		Uint32 raytracingVertexCount_ = 0;

		/// [EN] Offset (in units of float3) into Crister::raytracingMorphDeltaResource_
		///      where this SubMesh's target-major delta block starts
		///      ([target][local rt vertex], raytracingVertexCount_ deltas per
		///      target, morphs_.size() targets), or 0xFFFFFFFF when morphs_
		///      is empty. Runtime-only, rebuilt every load — not serialized.
		/// [JP] Crister::raytracingMorphDeltaResource_ 内で、この SubMesh の
		///      ターゲット主順デルタブロック([ターゲット][RT ローカル頂点]、
		///      ターゲットごとに raytracingVertexCount_ 個、morphs_.size() ターゲット分)
		///      が始まるオフセット(float3 単位)。morphs_ が空なら
		///      0xFFFFFFFF。実行時のみで、シリアライズしない。
		Uint32 raytracingMorphDeltaOffset_ = 0xFFFFFFFFu;

		/// [EN] Offset (in units of float3) into Crister::morphDeltaResource_
		///      where this SubMesh's target-major delta block starts
		///      ([target][local vertex, i.e. Morph::positionDeltas_'s own
		///      index], vertexCount_ deltas per target, morphs_.size()
		///      targets), or 0xFFFFFFFF when morphs_ is empty. Unlike
		///      raytracingMorphDeltaOffset_ above, this is baked straight from
		///      Morph::positionDeltas_ with no RT-proxy compaction — the
		///      raster path (SkeletalModelMS.hlsl/StaticModelMS.hlsl) reads
		///      it via Crister::vertexMorphSource_'s per-vertex remap
		///      instead, since raster streams every LOD (see
		///      vertexMorphSource_'s comment). Runtime-only, rebuilt every
		///      load — not serialized.
		/// [JP] Crister::morphDeltaResource_ 内で、この SubMesh の
		///      ターゲット主順デルタブロック([ターゲット][ローカル頂点、
		///      すなわち Morph::positionDeltas_ 自身のインデックス]、
		///      ターゲットごとに vertexCount_ 個、morphs_.size() ターゲット分)
		///      が始まるオフセット(float3単位)。morphs_ が空なら
		///      0xFFFFFFFF。上の raytracingMorphDeltaOffset_ と違い、RT プロキシの
		///      圧縮を経ず Morph::positionDeltas_ からそのまま焼き込む —
		///      ラスタ経路(SkeletalModelMS.hlsl/StaticModelMS.hlsl)は
		///      Crister::vertexMorphSource_ の頂点ごとの逆引きを介して読む
		///      (ラスタは全 LOD をストリームするため、vertexMorphSource_
		///      のコメント参照)。実行時のみで、シリアライズしない。
		Uint32 morphDeltaOffset_ = 0xFFFFFFFFu;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("cluster_offset", clusterOffset_);
			archive.Field("cluster_count", clusterCount_);
			archive.Field("material_index", materialIndex_);
			archive.Field("index_offset", indexOffset_);
			archive.Field("index_count", indexCount_);
			archive.Field("skin_index", skinIndex_);
			archive.Field("mesh_index", meshIndex_);
			archive.Field("morphs", morphs_);
			archive.Field("vertex_offset", vertexOffset_);
			archive.Field("vertex_count", vertexCount_);
		}
	};

	/**
	* [EN]
	* One glTF scene: a name plus the list of its root-level node
	* indices into nodes_. defaultStage_ selects which Stage renders by
	* default when none is explicitly chosen.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF の1シーンぶん: 名前と、nodes_ を指すルートレベルのノード
	* インデックス一覧。defaultStage_ が、明示的な選択が無い時に
	* デフォルトで描画される Stage を選ぶ。
	*/
	struct Stage
	{
		std::string name_;
		DynamicArray<Int> nodes_;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("name", name_);
			archive.Field("nodes", nodes_);
		}
	};

	/**
	* [EN]
	* Bundles every glTF KHR_materials_* extension a Material can carry.
	* Each extension's default is set so that "extension absent from the
	* glTF" and "extension present with default values" behave identically
	* (neutral / no effect).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF の KHR_materials_* 拡張をまとめて持つ。各拡張のデフォルトは
	* 「拡張が glTF に無い時＝中立（効果なし）」になるよう設定している。
	*/
	struct KHR
	{
		/**
		* [EN]
		* KHR_materials_emissive_strength: emissive intensity multiplier
		* (>1 for HDR emission). Default 1.0 = neutral.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_emissive_strength: エミッシブ強度（>1 で HDR 発光）。既定 1.0＝中立。
		*/
		struct EmissiveStrength
		{
			Float emissiveStrength_ = 1.0f;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("emissive_strength", emissiveStrength_);
			}
		};
		EmissiveStrength emissiveStrength_;

		/**
		* [EN]
		* KHR_materials_ior: dielectric index of refraction. Default 1.5
		* (glTF's own default). Feeds the F0 (Fresnel reflectance at normal
		* incidence) computation.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_ior: 誘電体の屈折率。既定 1.5（glTF 既定）。F0 計算に効く。
		*/
		struct Ior
		{
			Float ior_ = 1.5f;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("ior", ior_);
			}
		};
		Ior ior_;

		/**
		* [EN]
		* KHR_materials_specular: specular intensity and tint. Default
		* factor=1 / color=white = full specular (neutral).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_specular: スペキュラ強度と色。既定 factor=1/color=白＝フルスペキュラ（中立）。
		*/
		struct Specular
		{
			Float specularFactor_ = 1.0f;
			Float specularColorFactor_[3] = { 1,1,1 };

			Uint32 specularTextureIndex_ = 0xFFFFFFFF;
			Uint32 specularColorTextureIndex_ = 0xFFFFFFFF;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("specular_factor", specularFactor_);
				archive.Field("specular_color_factor", specularColorFactor_);
				archive.Field("specular_texture_index", specularTextureIndex_);
				archive.Field("specular_color_texture_index", specularColorTextureIndex_);
			}
		};
		Specular specular_;

		/**
		* [EN]
		* KHR_materials_clearcoat: an extra clear-coat lobe. Default
		* factor=0 = no coat (neutral).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_clearcoat: クリアコート層。既定 factor=0＝コート無し（中立）。
		*/
		struct ClearCoat
		{
			Float clearCoatFactor_ = 0.0f;
			Float clearCoatRoughnessFactor_ = 0.0f;

			Uint32 clearCoatTextureIndex_ = 0xFFFFFFFF;
			Uint32 clearCoatRoughnessTextureIndex_ = 0xFFFFFFFF;
			Uint32 clearCoatNormalTextureIndex_ = 0xFFFFFFFF;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("clear_coat_factor", clearCoatFactor_);
				archive.Field("clear_coat_roughness_factor", clearCoatRoughnessFactor_);
				archive.Field("clear_coat_texture_index", clearCoatTextureIndex_);
				archive.Field("clear_coat_roughness_texture_index", clearCoatRoughnessTextureIndex_);
				archive.Field("clear_coat_normal_texture_index", clearCoatNormalTextureIndex_);
			}
		};
		ClearCoat clearCoat_;

		/**
		* [EN]
		* KHR_materials_transmission: thin-glass transmission. Default
		* factor=0 = opaque (neutral).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_transmission: 透過（薄いガラス）。既定 factor=0＝不透過（中立）。
		*/
		struct Transmission
		{
			Float transmissionFactor_ = 0.0f;

			Uint32 transmissionTextureIndex_ = 0xFFFFFFFF;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("transmission_factor", transmissionFactor_);
				archive.Field("transmission_texture_index", transmissionTextureIndex_);
			}
		};
		Transmission transmission_;

		/**
		* [EN]
		* KHR_materials_volume: internal volume (thickness + absorption),
		* used together with Transmission. Default thickness=0 /
		* attenuationDistance=infinity (no absorption) / color=white =
		* neutral.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_volume: 内部ボリューム（厚み・吸収）。transmission と併用。
		* 既定 thickness=0/attenuationDistance=∞（吸収なし）/color=白＝中立。
		*/
		struct Volume
		{
			Float thicknessFactor_ = 0.0f;
			Float attenuationDistance_ = FLT_MAX;
			Float attenuationColor_[3] = { 1,1,1 };

			Uint32 thicknessTextureIndex_ = 0xFFFFFFFF;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("thickness_factor", thicknessFactor_);
				archive.Field("attenuation_distance", attenuationDistance_);
				archive.Field("attenuation_color", attenuationColor_);
				archive.Field("thickness_texture_index", thicknessTextureIndex_);
			}
		};
		Volume volume_;

		/**
		* [EN]
		* KHR_materials_sheen: fabric-like sheen lobe. Default color=black =
		* no sheen (neutral).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_sheen: 布の光沢。既定 color=黒＝シーン無し（中立）。
		*/
		struct Sheen
		{
			Float sheenColorFactor_[3] = { 0,0,0 };
			Float sheenRoughnessFactor_ = 0.0f;

			Uint32 sheenColorTextureIndex_ = 0xFFFFFFFF;
			Uint32 sheenRoughnessTextureIndex_ = 0xFFFFFFFF;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("sheen_color_factor", sheenColorFactor_);
				archive.Field("sheen_roughness_factor", sheenRoughnessFactor_);
				archive.Field("sheen_color_texture_index", sheenColorTextureIndex_);
				archive.Field("sheen_roughness_texture_index", sheenRoughnessTextureIndex_);
			}
		};
		Sheen sheen_;

		/**
		* [EN]
		* KHR_materials_iridescence: thin-film iridescence (soap bubble /
		* oil slick). Default factor=0 = none (neutral). Thickness is in
		* nanometres (default min=100/max=400), ior default 1.3.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_iridescence: 虹色（シャボン玉・油膜）。既定 factor=0＝無し（中立）。
		* 厚みは nm 単位（既定 min=100/max=400）、ior 既定 1.3。
		*/
		struct Iridescence
		{
			Float iridescenceFactor_ = 0.0f;
			Float iridescenceIor_ = 1.3f;
			Float iridescenceThicknessMinimum_ = 100.0f;
			Float iridescenceThicknessMaximum_ = 400.0f;

			Uint32 iridescenceTextureIndex_ = 0xFFFFFFFF;
			Uint32 iridescenceThicknessTextureIndex_ = 0xFFFFFFFF;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("iridescence_factor", iridescenceFactor_);
				archive.Field("iridescence_ior", iridescenceIor_);
				archive.Field("iridescence_thickness_minimum", iridescenceThicknessMinimum_);
				archive.Field("iridescence_thickness_maximum", iridescenceThicknessMaximum_);
				archive.Field("iridescence_texture_index", iridescenceTextureIndex_);
				archive.Field("iridescence_thickness_texture_index", iridescenceThicknessTextureIndex_);
			}
		};
		Iridescence iridescence_;

		/**
		* [EN]
		* KHR_materials_anisotropy: anisotropic specular (brushed metal,
		* hair). Default strength=0 = isotropic (neutral). rotation is in
		* radians.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_anisotropy: 異方性スペキュラ（ヘアライン金属・髪）。
		* 既定 strength=0＝等方（中立）。rotation はラジアン。
		*/
		struct Anisotropy
		{
			Float anisotropyStrength_ = 0.0f;
			Float anisotropyRotation_ = 0.0f;

			Uint32 anisotropyTextureIndex_ = 0xFFFFFFFF;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("anisotropy_strength", anisotropyStrength_);
				archive.Field("anisotropy_rotation", anisotropyRotation_);
				archive.Field("anisotropy_texture_index", anisotropyTextureIndex_);
			}
		};
		Anisotropy anisotropy_;

		/**
		* [EN]
		* KHR_materials_unlit: disables lighting (flat shading). 1 when the
		* extension is present, default 0 = normal lighting.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_materials_unlit: ライティング無効（フラット）。存在で 1、既定 0＝通常ライティング。
		*/
		struct Unlit
		{
			Int unlit_ = 0;

			template<class Archive>
			void Serialize(Archive& archive)
			{
				archive.Field("unlit", unlit_);
			}
		};
		Unlit unlit_;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("emissive_strength", emissiveStrength_);
			archive.Field("ior", ior_);
			archive.Field("specular", specular_);
			archive.Field("clear_coat", clearCoat_);
			archive.Field("transmission", transmission_);
			archive.Field("volume", volume_);
			archive.Field("sheen", sheen_);
			archive.Field("iridescence", iridescence_);
			archive.Field("anisotropy", anisotropy_);
			archive.Field("unlit", unlit_);
		}
	};

	/**
	* [EN]
	* Which lighting response DeferredLightingPS.hlsl evaluates a material's
	* direct/indirect light with. Not a separate shader/PSO - the deferred
	* lighting pass reads this per-instance and switches within the single
	* shader (the same technique khr_.unlit_ already used before this enum
	* existed; Unlit now folds into this list instead of its own flag).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* DeferredLightingPS.hlsl がマテリアルの直接光/間接光をどのライティング
	* 応答で評価するか。別シェーダー/PSOではない — デフォードライティング
	* パスがinstanceごとにこれを読み、単一シェーダー内でswitchする(この
	* enumができる前からkhr_.unlit_が使っていたのと同じ手法。Unlitは
	* 専用フラグをやめてこのリストへ統合された)。
	*/
	enum class ShadingModel :Uint32
	{
		Pbr = 0,
		Unlit = 1,
		Phong = 2,
		Toon = 3,
	};

	/**
	* [EN]
	* glTF PBR metallic-roughness material: base factors, alpha mode/
	* cutoff/double-sidedness, bundled KHR_materials_* extensions (khr_),
	* and bindless-heap-agnostic texture indices (resolved into actual
	* bindless indices at Upload() time via TextureBindlessIndex).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF の PBR metallic-roughness マテリアル: 基本ファクタ、アルファ
	* モード/カットオフ/両面描画フラグ、まとめた KHR_materials_* 拡張
	* (khr_)、そして bindless ヒープに依存しないテクスチャインデックス
	* （Upload() 時に TextureBindlessIndex 経由で実際の bindless
	* インデックスへ解決される）。
	*/
	struct Material
	{
		Color baseColor_ = { 1,1,1,1 };
		Float metallic_ = 0.0f;
		Float roughness_ = 1.0f;
		Float emissiveFactor_[3] = { 0,0,0 };
		Int alphaMode_ = 0;
		Float alphaCutoff_ = 0.5f;
		Int doubleSided_ = 0;

		/// [EN] Overridden to ShadingModel::Unlit at Gather time when
		///      khr_.unlit_.unlit_ is set (see ModelRenderer::Gather) - the
		///      glTF extension always wins over whatever is authored here.
		/// [JP] khr_.unlit_.unlit_ が立っている場合、Gather時にShadingModel::Unlit
		///      へ上書きされる(ModelRenderer::Gather参照) - glTF拡張が、
		///      ここで設定された値より常に優先される。
		ShadingModel shadingModel_ = ShadingModel::Pbr;

		/// [JP] KHR_materials_* 拡張。拡張が無ければ各デフォルト（中立値）のまま。
		KHR khr_;

		Uint32 baseColorTextureIndex_ = 0xFFFFFFFF;
		Uint32 normalTextureIndex_ = 0xFFFFFFFF;
		Uint32 metallicRoughnessTextureIndex_ = 0xFFFFFFFF;
		Uint32 occlusionTextureIndex_ = 0xFFFFFFFF;
		Uint32 emissiveTextureIndex_ = 0xFFFFFFFF;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("base_color", baseColor_);
			archive.Field("metallic", metallic_);
			archive.Field("roughness", roughness_);
			archive.Field("emissive_factor", emissiveFactor_);
			archive.Field("alpha_mode", alphaMode_);
			archive.Field("alpha_cutoff", alphaCutoff_);
			archive.Field("double_sided", doubleSided_);
			archive.Field("shading_model", shadingModel_);
			archive.Field("khr", khr_);
			archive.Field("base_color_texture_index", baseColorTextureIndex_);
			archive.Field("normal_texture_index", normalTextureIndex_);
			archive.Field("metallic_roughness_texture_index", metallicRoughnessTextureIndex_);
			archive.Field("occlusion_texture_index", occlusionTextureIndex_);
			archive.Field("emissive_texture_index", emissiveTextureIndex_);
		}
	};

	/**
	* [EN]
	* One node in the flattened glTF node hierarchy: local S/R/T, an
	* optional mesh_/skin_/light_ reference, and children_ indices into
	* nodes_. globalTransform_ is cumulated top-down by
	* CumulateTransforms()/ModelLoader::CumulateTransforms from every
	* ancestor's local transform.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 平坦化された glTF ノード階層の1ノード: ローカル S/R/T、任意の
	* mesh_/skin_/light_ 参照、nodes_ を指す children_ インデックス。
	* globalTransform_ は CumulateTransforms()/
	* ModelLoader::CumulateTransforms が全祖先のローカルトランスフォーム
	* から上から下へ累積計算する。
	*/
	struct Node
	{
		std::string name_;
		Int mesh_ = -1;
		Int skin_ = -1;

		/// [EN] KHR_lights_punctual: index into Crister::lights_, -1 if none.
		/// [JP] KHR_lights_punctual: Crister::lights_ へのインデックス。無ければ -1。
		Int light_ = -1;
		DynamicArray<Int> children_;

		/// [EN] Index into Crister::nodes_ of this node's parent, -1 for a
		///      root node. Fully derivable from every node's children_, so
		///      not serialized — ModelLoader::FetchNodes recomputes it every
		///      load in a pass over the already-populated children_ arrays.
		/// [JP] この Node の親への Crister::nodes_ インデックス、ルートなら
		///      -1。全ノードの children_ から完全に導出できるためシリアライズ
		///      しない — ModelLoader::FetchNodes が、埋め終わった children_
		///      配列を走査して毎ロード再計算する。
		Int parentIndex_ = -1;

		Quaternion rotation_ = { 0,0,0,1 };
		Vector3 scale_ = { 1,1,1 };
		Vector3 translation_ = { 0,0,0 };
		Matrix globalTransform_ = Matrix::Identity;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("name", name_);
			archive.Field("mesh", mesh_);
			archive.Field("skin", skin_);
			archive.Field("light", light_);
			archive.Field("children", children_);
			archive.Field("rotation", rotation_);
			archive.Field("scale", scale_);
			archive.Field("translation", translation_);
			archive.Field("global_transform", globalTransform_);
		}
	};

	/**
	* [EN]
	* glTF skin: the joint list (indices into nodes_) and their matching
	* inverse-bind matrices, indexed 1:1. Referenced by Node::skin_ and
	* SubMesh::skinIndex_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF のスキン: ジョイント一覧（nodes_ へのインデックス）と、それに
	* 1対1対応する逆バインド行列。Node::skin_ と SubMesh::skinIndex_ から
	* 参照される。
	*/
	struct Skin
	{
		DynamicArray<Matrix> inverseBindMatrices_;
		DynamicArray<Int> joints_;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("inverse_bind_matrices", inverseBindMatrices_);
			archive.Field("joints", joints_);
		}
	};

	/**
	* [EN]
	* KHR_lights_punctual point/spot light, resolved to world-space
	* position/direction (via the referencing Node's globalTransform_)
	* at load time. Directional lights are not represented here; see
	* ModelLoader::FetchLights.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* KHR_lights_punctual のポイント/スポットライト。ロード時に参照ノードの
	* globalTransform_ からワールド空間の位置/向きに解決済み。
	* ディレクショナルライトはここに含めない（ModelLoader::FetchLights 参照）。
	*/
	struct PunctualLight
	{
		/**
		* [EN]
		* Which KHR_lights_punctual light kind this PunctualLight represents.
		* Directional is not a value here since directional lights are not
		* represented by PunctualLight at all (see the struct comment above).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この PunctualLight が表す KHR_lights_punctual のライト種別。
		* ディレクショナルは PunctualLight そのもので表現しない
		* （上の構造体コメント参照）ため、値として存在しない。
		*/
		enum class Type : Uint32
		{
			/// [EN] Omnidirectional point light.
			/// [JP] 全方向へ発光するポイントライト。
			Point = 0,

			/// [EN] Cone-shaped spot light (uses direction_/innerConeAngle_/outerConeAngle_).
			/// [JP] 円錐状に発光するスポットライト（direction_/innerConeAngle_/outerConeAngle_ を使う）。
			Spot = 1,
		};

		Type type_ = Type::Point;
		Vector3 position_ = { 0,0,0 };
		Vector3 direction_ = { 0,0,-1 }; // Spot only.
		Float colorRGB_[3] = { 1,1,1 };
		Float intensity_ = 1.0f;
		Float range_ = 0.0f; // glTF: 0 = unbounded; resolved to a finite fallback by FetchLights.
		Float innerConeAngle_ = 0.0f; // Spot only, radians.
		Float outerConeAngle_ = 0.785398163f; // Spot only, radians (45 deg default per glTF spec).

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("type", type_);
			archive.Field("position", position_);
			archive.Field("direction", direction_);
			archive.Field("color_rgb", colorRGB_);
			archive.Field("intensity", intensity_);
			archive.Field("range", range_);
			archive.Field("inner_cone_angle", innerConeAngle_);
			archive.Field("outer_cone_angle", outerConeAngle_);
		}
	};

	/**
	* [EN]
	* width_/height_/mipCount_ describe cacheData_ as baked by
	* Crister::BakeBitmap(): BC7-compressed blocks for mip 0 through
	* mipCount_-1, concatenated in mip order (standard BC block
	* layout — 16 bytes per 4x4 texel block, no explicit per-mip
	* offsets stored; Upload() recomputes them from width_/height_).
	* component_/bits_ describe the original glTF source image only.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* width_/height_/mipCount_ は Crister::BakeBitmap() が焼いた
	* cacheData_ の内容を表す: mip 0 から mipCount_-1 までの BC7 圧縮
	* ブロックをミップ順に連結したもの（標準的な BC ブロックレイアウト
	* — 4x4 テクセルブロックあたり 16 バイト、ミップごとのオフセットは
	* 持たず Upload() が width_/height_ から再計算する）。
	* component_/bits_ は元の glTF ソース画像の情報のみ。
	*/
	struct Bitmap
	{
		std::string name_;
		Int width_ = -1;
		Int height_ = -1;
		Int component_ = -1;
		Int bits_ = -1;
		Int mipCount_ = 1;
		std::string mimeType_;
		DynamicArray<Uchar> cacheData_;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("name", name_);
			archive.Field("width", width_);
			archive.Field("height", height_);
			archive.Field("component", component_);
			archive.Field("bits", bits_);
			archive.Field("mip_count", mipCount_);
			archive.Field("mime_type", mimeType_);
			archive.Field("cache_data", cacheData_);
		}
	};

	/**
	* [EN]
	* Which cluster BakeCollision pulls from a SubMesh: the coarsest
	* cluster (same range RT already uses for its proxy geometry) or
	* the LOD 0 cluster.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* BakeCollision が SubMesh のどのクラスタから抽出するか:
	* 最粗クラスタ（RT のプロキシジオメトリと同じ範囲）か LOD 0 クラスタか。
	*/
	enum class MeshCollisionDetail
	{
		/// [EN] Each SubMesh's coarsest cluster — same range the RT proxy
		///      geometry already uses.
		/// [JP] 各 SubMesh の最粗クラスタ — RT プロキシジオメトリが既に
		///      使っているのと同じ範囲。
		Proxy,

		/// [EN] Each SubMesh's LOD 0 (most detailed) cluster.
		/// [JP] 各 SubMesh の LOD 0（最も詳細な）クラスタ。
		Exact,
	};

	/**
	* [EN]
	* GPU-resident, geometry/texture-streaming model asset: quantised
	* mesh data (compressedVertices_ etc.), meshlet/cluster LOD hierarchy,
	* materials/nodes/skins/lights, and the .crister-cache-backed bake
	* pipeline (BakeMesh/BakeBitmap) that produces them. Built once by
	* ModelLoader from a source glTF (or reloaded straight from a
	* .crister cache), then Upload()ed to create its D3D12 resources and
	* bindless indices. Owns its own Nanite-style streaming residency
	* (MakeClusterResident/EvictCluster, MakeTextureMipResident/
	* EvictTextureMip) against shared VRAM budgets tracked across every
	* live Crister.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GPU 常駐・ジオメトリ/テクスチャストリーミング対応のモデルアセット:
	* 量子化済みメッシュデータ (compressedVertices_ 等)、meshlet/cluster
	* の LOD 階層、マテリアル/ノード/スキン/ライト、そしてそれらを生成する
	* .crister キャッシュ向けのベイクパイプライン (BakeMesh/BakeBitmap)。
	* ModelLoader がソース glTF から一度構築する（あるいは .crister
	* キャッシュから直接リロードする）。その後 Upload() で D3D12
	* リソースと bindless インデックスを作成する。独自の Nanite 型
	* ストリーミング常駐管理 (MakeClusterResident/EvictCluster、
	* MakeTextureMipResident/EvictTextureMip) を持ち、生存中の全 Crister
	* で共有する VRAM 予算に対して管理する。
	*/
	class SEEDCORE_API Crister
	{
	private:
		friend class ModelLoader;

		/// [EN] Source vertices. Only alive during the load/bake pipeline
		///      (FetchMeshes -> BuildMeshlets -> BakeMesh); not serialized.
		///      BakeMesh() consumes this into compressedVertices_/
		///      compressedSkinVertices_ and clears it.
		/// [JP] ソース頂点。ロード/ベイクのパイプライン中
		///      (FetchMeshes -> BuildMeshlets -> BakeMesh) のみ生存し、
		///      シリアライズしない。BakeMesh() がこれを compressedVertices_/
		///      compressedSkinVertices_ へ変換して空にする。
		DynamicArray<Vertex> vertices_;

		/// [EN] For each entry in vertices_/compressedVertices_ (including
		///      LOD-duplicated copies BuildMeshlets appends), the ORIGINAL
		///      (pre-LOD) vertex index it descends from — identity for an
		///      original vertex, and propagated forward from the source
		///      vertex BuildMeshlets' QEM step copied attributes from
		///      otherwise. Only meaningful within a SubMesh's own range.
		///      Unlike vertices_ itself, this IS serialized — it stays
		///      index-aligned with compressedVertices_ (also serialized),
		///      so both must survive a cache load equally; BakeMesh() does
		///      NOT clear this alongside vertices_. Exists solely so the
		///      raster morph blend path (SkeletalModelMS.hlsl/
		///      StaticModelMS.hlsl, via Model.hlsli's ApplyMorphBlend) can
		///      resolve ANY streamed LOD's vertex back to the original
		///      vertex Morph::positionDeltas_ is index-aligned to — RT's
		///      morph blend needs no such remap since its proxy only ever
		///      uses each SubMesh's original (LOD-0/full-detail) cluster.
		/// [JP] vertices_/compressedVertices_ の各要素(BuildMeshlets が
		///      追加する LOD 複製コピーも含む)について、その元となった
		///      (LOD 適用前の)頂点インデックス — オリジナル頂点なら恒等、
		///      それ以外は BuildMeshlets の QEM ステップが属性をコピーした
		///      元頂点から伝播する。SubMesh 自身の範囲内でのみ意味を持つ。
		///      vertices_ 自身と違い、これはシリアライズする —
		///      compressedVertices_(同じくシリアライズ)とインデックスが
		///      整合し続ける必要があるため、両者ともキャッシュロードを
		///      同じように生き延びる必要がある。BakeMesh() は vertices_ と
		///      一緒にこれをクリアしない。ラスタのモーフブレンドパス
		///      (SkeletalModelMS.hlsl/StaticModelMS.hlsl、Model.hlsli の
		///      ApplyMorphBlend 経由)が、ストリームされたどの LOD の頂点も
		///      Morph::positionDeltas_ がインデックス整合するオリジナル
		///      頂点へ逆引きできるようにする用途のみに存在する — RT の
		///      モーフブレンドは、そのプロキシが各 SubMesh のオリジナル
		///      (LOD0/フル詳細)クラスタしか使わないため、この逆引きを
		///      必要としない。
		DynamicArray<Uint32> vertexMorphSource_;

		/// [EN] Quantised GPU vertex format, baked by BakeMesh() and cached to
		///      disk. Indexed the same way vertices_ was (global vertex index).
		/// [JP] 量子化済み GPU 頂点フォーマット。BakeMesh() で焼き込みディスクに
		///      キャッシュする。vertices_ と同じグローバル頂点インデックスで引く。
		DynamicArray<CompressedVertex> compressedVertices_;

		/// [EN] Quantised skinning attributes, baked alongside compressedVertices_.
		///      Empty when skins_ is empty.
		/// [JP] 量子化済みスキニング属性。compressedVertices_ と一緒に焼き込む。
		///      skins_ が空なら空のまま。
		DynamicArray<CompressedSkinVertex> compressedSkinVertices_;

		DynamicArray<Uint32> vertexIndices_;
		DynamicArray<Uint8> primitiveIndices_;
		DynamicArray<Meshlet> meshlets_;
		DynamicArray<MeshletBound> meshletBounds_;
		DynamicArray<Cluster> clusters_;
		DynamicArray<Stage> stages_;
		DynamicArray<SubMesh> subMeshes_;
		DynamicArray<Material> materials_;
		DynamicArray<Node> nodes_;
		DynamicArray<Skin> skins_;
		DynamicArray<Bitmap> bitmaps_;
		DynamicArray<PunctualLight> lights_;

		Int defaultStage_ = 0;

	public:
		Crister() = default;

		/**
		* [EN]
		* Releases every resident streaming page (pinned included — the model
		* itself is going away) and returns their descriptor indices to
		* bindlessHeap_. Every GPU resource is handed to the deferred-reclaim
		* ring rather than dying with this object, since frames still in
		* flight may be drawing from these exact buffers and textures.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 常駐中のストリーミングページをすべて解放し（モデル自体が消えるため
		* ピン留め込み）、ディスクリプタインデックスを bindlessHeap_ へ返す。
		* GPU リソースはこのオブジェクトと一緒に死なせず、遅延回収リングへ
		* 渡す — インフライトのフレームがまさにこれらのバッファやテクスチャで
		* 描画している可能性があるため。
		*/
		~Crister();

		Crister(Crister&&)noexcept = default;
		Crister& operator=(Crister&&)noexcept = default;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("compressed_vertices", compressedVertices_);
			archive.Field("compressed_skin_vertices", compressedSkinVertices_);
			archive.Field("vertex_morph_source", vertexMorphSource_);
			archive.Field("position_min", positionMin_);
			archive.Field("position_extent", positionExtent_);
			archive.Field("texcoord_min", texcoordMin_);
			archive.Field("texcoord_extent", texcoordExtent_);
			archive.Field("vertex_indices", vertexIndices_);
			archive.Field("primitive_indices", primitiveIndices_);
			archive.Field("meshlets", meshlets_);
			archive.Field("meshlet_bounds", meshletBounds_);
			archive.Field("clusters", clusters_);
			archive.Field("stages", stages_);
			archive.Field("sub_meshes", subMeshes_);
			archive.Field("materials", materials_);
			archive.Field("nodes", nodes_);
			archive.Field("skins", skins_);
			archive.Field("bitmaps", bitmaps_);
			archive.Field("lights", lights_);
			archive.Field("default_stage", defaultStage_);
		}

		/**
		* [EN]
		* Computes the quantisation AABB and bakes vertices_ (and, if
		* skinned, skin attributes) into compressedVertices_ /
		* compressedSkinVertices_, then frees vertices_. Must run after
		* BuildMeshlets (LOD vertex duplication must already be final)
		* and before serialising to the .crister cache.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 量子化 AABB を計算し、vertices_（スキンがあればスキン属性も）を
		* compressedVertices_ / compressedSkinVertices_ へ焼き込んで
		* vertices_ を解放する。BuildMeshlets の後（LOD 頂点複製が確定済み）、
		* .crister キャッシュへのシリアライズ前に実行すること。
		*/
		void BakeMesh();

		/**
		* [EN]
		* Downsamples oversized textures, dilates transparent-texel color
		* (must happen before compression — BC7 blocks can't be touched
		* per-texel afterwards), then BC7-compresses a full mip chain into
		* each Bitmap's cacheData_. Must run before serialising to the
		* .crister cache.
		* BC7 encoding runs on the GPU (BC7CompressCS.hlsl, mode 6 only —
		* the CPU DirectXTex encoder's default quality search is
		* impractically slow and TEX_COMPRESS_PARALLEL is a no-op unless
		* the vendored DirectXTex.lib happens to be built with OpenMP).
		* Needs device/cmdQueue for the one-shot compute dispatch; unlike
		* Upload() this has no BindlessHeap dependency (the shader binds
		* its input/output as root SRV/UAV, not through the bindless heap).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 大きすぎるテクスチャをダウンサンプルし、透明テクセルの色を
		* dilation する（圧縮後は BC7 ブロックをテクセル単位で触れない
		* ため圧縮前必須）。その後フルミップチェーンを BC7 圧縮して各
		* Bitmap の cacheData_ へ焼き込む。.crister キャッシュへの
		* シリアライズ前に実行すること。
		* BC7 エンコードは GPU で行う(BC7CompressCS.hlsl、mode 6 のみ —
		* CPU 版 DirectXTex のデフォルト品質探索は実用にならないほど遅く、
		* TEX_COMPRESS_PARALLEL も同梱の DirectXTex.lib が OpenMP 付きで
		* ビルドされていない限り無効)。一発実行のコンピュートディスパッチの
		* ため device/cmdQueue が要る。Upload() と違い BindlessHeap には
		* 依存しない(シェーダの入出力は bindless ヒープではなくルート
		* SRV/UAV で直接バインドする)。ルートシグネチャ+PSOは
		* BC7CompressShader(Graphics所有、ModelShaderと同じ立ち位置)が
		* 持つので、それを渡してもらう。
		*/
		void BakeBitmap(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BC7CompressShader& bc7Shader);

		/**
		* [EN]
		* Flattens this Crister's triangles into a CPU-side position/index
		* pair, for MeshCollisionLoader to bake into a ".collision" file.
		* Reads only the CPU-resident arrays (compressedVertices_/
		* meshlets_/vertexIndices_/primitiveIndices_/clusters_), so it is
		* unaffected by geometry streaming residency and needs no GPU
		* readback. Positions are dequantised with the same math as the
		* shader decode. Returns false when there is nothing to extract.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この Crister の三角形を CPU 側の位置/インデックス対へ展開する。
		* MeshCollisionLoader がこれを ".collision" ファイルへ焼き込む。
		* CPU 常駐配列 (compressedVertices_/meshlets_/vertexIndices_/
		* primitiveIndices_/clusters_) しか読まないため、ジオメトリ
		* ストリーミングの常駐状態に影響されず、GPU リードバックも不要。
		* 位置はシェーダデコードと同じ計算で逆量子化する。抽出対象が
		* 無ければ false を返す。
		*/
		[[nodiscard]] Bool BakeCollision(MeshCollisionDetail detail, DynamicArray<Vector3>& outPositions, DynamicArray<Uint32>& outIndices)const;

		/**
		* [EN]
		* Reads this Crister's LOD 0 geometry into a CPU-side full-vertex
		* (position/normal/tangent/texcoord)/index pair addressed by the
		* SAME global vertex index space compressedVertices_/vertexIndices_
		* already use — decoded from compressedVertices_ (there is no
		* surviving full-precision source once BakeMesh() has run). Unlike
		* BakeCollision (which compacts/dedups into its own local index
		* space and is baked once into the cached ".collision" asset), this
		* is not part of the bake/cache pipeline — nothing is written to
		* disk, and Softbody calls it directly off the already-loaded
		* Crister once, to seed both the physics simulation (positions) and
		* the render-side SoftbodyMesh (normal_/tangent_/texcoord_, which
		* stay at their bind pose — the simulation only ever overwrites
		* position_ each frame; see SoftbodyMesh). outVertices is a 1:1,
		* unfiltered copy of compressedVertices_ and outIndices reference it
		* directly, so a physical simulation can run on the exact same
		* vertex ordering the mesh shader reads and write simulated vertex i
		* straight back into the render vertex buffer at index i with no
		* remap step. Reads only the CPU-resident arrays, same as
		* BakeCollision. Returns false when there is nothing to extract.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この Crister の LOD 0 ジオメトリを、compressedVertices_/
		* vertexIndices_ が既に使っているのと同じグローバル頂点インデックス
		* 空間で、CPU 側のフル頂点（位置/法線/接線/UV）/インデックス対へ
		* 読み出す — compressedVertices_ からのデコードになる（BakeMesh()
		* 実行後はフル精度のソースが残っていないため）。BakeCollision
		* （独自のローカルインデックス空間へ圧縮・重複排除し、".collision"
		* アセットとして一度だけ焼き込む）と異なり、これはベイク/キャッシュ
		* パイプラインの一部ではない — ディスクには何も書き込まず、
		* Softbody が一度だけロード済みの Crister に対して直接呼び出し、
		* 物理シミュレーション（位置）とレンダー側の SoftbodyMesh
		* （normal_/tangent_/texcoord_ — バインドポーズのまま使い回す。
		* シミュレーションは毎フレーム position_ だけを上書きする。
		* SoftbodyMesh 参照）の両方の種にする。outVertices は
		* compressedVertices_ の 1:1 な無加工コピーで、outIndices はそれを
		* 直接参照する。これにより物理シミュレーションがメッシュシェーダと
		* 全く同じ頂点順序で動作でき、シミュレート後の頂点 i をそのまま
		* レンダー用頂点バッファのインデックス i へ書き戻せる（リマップ
		* 不要）。BakeCollision と同じく CPU 常駐配列のみを読む。抽出対象が
		* 無ければ false を返す。
		*/
		[[nodiscard]] Bool SoftbodyFinestVertices(DynamicArray<Vertex>& outVertices, DynamicArray<Uint32>& outIndices)const;

		/**
		* [EN]
		* Reads each SubMesh's coarsest cluster (same range BakeCollision's
		* Proxy detail and the RT proxy geometry use) into a CPU-side
		* full-vertex/index pair, compacted/deduped into its own local
		* index space (unlike SoftbodyFinestVertices, which keeps the full LOD 0
		* global index space uncompacted). For a Softbody to stay real-time
		* on a render-resolution mesh (thousands of vertices, one PBD edge/
		* shear/bend constraint per edge — far past what Jolt's soft body
		* solver is meant for), simulation runs on this much smaller proxy
		* mesh instead; SoftbodyMesh binds each render vertex (from
		* SoftbodyFinestVertices) to its nearest proxy vertices at build time and
		* blends their simulated displacement onto the full-resolution mesh
		* every frame, so the mesh shader still renders full detail. Not
		* part of the bake/cache pipeline — called live off the
		* already-loaded Crister. Returns false when there is nothing to
		* extract.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 各 SubMesh の最粗クラスタ（BakeCollision の Proxy 詳細度や RT
		* プロキシジオメトリと同じ範囲）を、CPU 側のフル頂点/インデックス対
		* へ、自身のローカルインデックス空間に圧縮・重複排除して読み出す
		* （LOD 0 のグローバルインデックス空間を無加工のまま保つ
		* SoftbodyFinestVertices とは異なる）。Softbody がレンダー解像度の
		* メッシュ（数千頂点、辺ごとに PBD の辺/シア/曲げ拘束1つ —
		* Jolt のソフトボディソルバーが想定する規模をはるかに超える）で
		* リアルタイムを保つため、シミュレーションはこのずっと小さい
		* プロキシメッシュ上で行う。SoftbodyMesh がビルド時に各レンダー
		* 頂点（SoftbodyFinestVertices 由来）を最も近いプロキシ頂点群へ束縛し、
		* 毎フレームそのシミュレート済み変位をブレンドしてフル解像度
		* メッシュへ適用するので、メッシュシェーダは引き続きフル
		* ディテールで描画する。ベイク/キャッシュパイプラインの一部では
		* なく、ロード済みの Crister に対してその都度呼び出す。抽出対象が
		* 無ければ false を返す。
		*/
		[[nodiscard]] Bool SoftbodyCoarsestVertices(DynamicArray<Vertex>& outVertices, DynamicArray<Uint32>& outIndices)const;

		/**
		* [EN]
		* Quantises a single full-precision Vertex into the 16-byte GPU
		* format, given the position/texcoord dequantisation AABBs to
		* quantise against. Extracted out of BakeMesh()'s per-vertex loop
		* body so SoftbodyMesh can re-quantise a Softbody's deformed
		* vertices every frame against a freshly computed AABB (the mesh
		* moves, so a fixed bake-time AABB would clip) using the exact same
		* math BakeMesh() bakes into the cache with — must stay in sync with
		* Model.hlsli's DecodeModelVertex, same as BakeMesh().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* フル精度の Vertex 1 つを、指定された位置/UV の逆量子化 AABB に
		* 対して 16 バイトの GPU フォーマットへ量子化する。BakeMesh() の
		* 頂点ループ本体から切り出したもの — SoftbodyMesh が Softbody の
		* 変形後頂点を毎フレーム、その都度計算した AABB（メッシュが動くため
		* 焼き込み時固定の AABB ではクリップしてしまう）に対して、
		* BakeMesh() がキャッシュへ焼き込むのと全く同じ計算式で再量子化
		* できるようにする — BakeMesh() と同様、Model.hlsli の
		* DecodeModelVertex と同期を保つこと。
		*/
		[[nodiscard]] static CompressedVertex EncodeVertex(const Vertex& vertex, const Vector3& positionMin, const Vector3& positionExtent, const Vector2& texcoordMin, const Vector2& texcoordExtent);

		/**
		* [EN]
		* Inverse of EncodeVertex: dequantises a CompressedVertex back into a
		* full-precision Vertex, against this Crister's own quantisation AABBs
		* (positionMin_/positionExtent_/texcoordMin_/texcoordExtent_) — unlike
		* EncodeVertex, not static, since every call site decodes this
		* Crister's own compressedVertices_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* EncodeVertex の逆。CompressedVertex を、この Crister 自身の量子化
		* AABB（positionMin_/positionExtent_/texcoordMin_/texcoordExtent_）に
		* 対してフル精度の Vertex へ逆量子化する。EncodeVertex と異なり
		* static ではない — どの呼び出し元もこの Crister 自身の
		* compressedVertices_ をデコードするため。
		*/
		[[nodiscard]] Vertex DecodeVertex(const CompressedVertex& compressed)const;

		/**
		* [EN]
		* Packs a unit direction into the 16+16-bit octahedral encoding
		* (Shader/Normal.hlsli::OctNormalEncode's CPU-side counterpart).
		* Static for the same reason as EncodeVertex: SoftbodyMesh calls this
		* indirectly via EncodeVertex with its own bounds, not this Crister's.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 単位方向ベクトルを 16+16bit のオクタヘドラル符号化へ詰める
		* （Shader/Normal.hlsli::OctNormalEncode の CPU 側対応）。EncodeVertex
		* と同じ理由で static — SoftbodyMesh が EncodeVertex 経由で、この
		* Crister とは別の自身の境界を使って間接的に呼ぶため。
		*/
		[[nodiscard]] static Uint32 EncodeOctahedralNormal(Vector3 direction);

		/**
		* [EN]
		* Inverse of EncodeOctahedralNormal (Shader/Normal.hlsli::OctNormalDecode's
		* CPU-side counterpart). Not static, matching DecodeVertex.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* EncodeOctahedralNormal の逆（Shader/Normal.hlsli::OctNormalDecode の
		* CPU 側対応）。DecodeVertex と同じく static ではない。
		*/
		[[nodiscard]] Vector3 DecodeOctahedralNormal(Uint32 packed)const;

		/**
		* [EN]
		* Re-converts an already-baked Crister (no source glTF required) from
		* one axis convention to another, in place: decodes every quantised
		* vertex/skin back to full precision, applies deltaBasis to
		* positions/normals/tangents/node transforms/skin inverse-bind
		* matrices/light positions-directions/meshlet bounds, optionally
		* reverses triangle winding, recomputes the quantisation AABBs from
		* the transformed data, re-quantises via BakeMesh(), and
		* re-serialises the result to cristerPath. Does NOT touch GPU
		* resources or bindless indices — the caller must force a reload
		* (Unload then Load) afterward. Returns false if this Crister has no
		* compressed vertex data to convert (e.g. textures-only/degenerate asset).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 既に焼き込み済みの Crister（ソース glTF 不要）を、ある軸コンベンション
		* から別のものへその場で再変換する: 量子化済みの頂点/スキンをフル精度へ
		* デコードし、位置/法線/タンジェント/ノードトランスフォーム/スキン
		* 逆バインド行列/ライト位置・向き/メシュレット境界へ deltaBasis を
		* 適用、必要なら三角形の巻き順を反転、変換後データから量子化 AABB を
		* 再計算し、BakeMesh() で再量子化して cristerPath へ再シリアライズ
		* する。GPU リソースや bindless インデックスは触らない —
		* 呼び出し側が後で強制リロード（Unload → Load）すること。この
		* Crister に変換対象の量子化済み頂点データが無い場合（テクスチャのみ
		* 等の縮退アセット）は false を返す。
		*/
		Bool ApplyAxisConversion(const Matrix& deltaBasis, Bool flipWinding, const std::filesystem::path& cristerPath);

		/**
		* [EN]
		* Bakes a global position/rotation(euler degrees)/scale/pivot
		* transform into this Crister's data, same scope as
		* ApplyAxisConversion (vertices/node hierarchy/skin inverse-bind
		* matrices/light positions-directions/meshlet bounds), then
		* re-serialises to cristerPath. scale/pivot/rotation compose about
		* pivot first, position is a separate world-space offset applied
		* after. Only root-level nodes (stages_[defaultStage_].nodes_) have
		* their local transform updated - CumulateTransforms() then
		* propagates to every descendant, since post-multiplying the whole
		* transform onto just the root telescopes correctly through the
		* local-transform chain (node.globalTransform_ = local *
		* parentGlobal). Returns false if this Crister has no compressed
		* vertex data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グローバルな位置/回転(オイラー角、度)/スケール/ピボット変換を
		* この Crister のデータへ焼き込む。対象範囲は ApplyAxisConversion
		* と同じ(頂点/ノード階層/スキン逆バインド行列/ライト位置・向き/
		* メシュレット境界)、その後 cristerPath へ再シリアライズする。
		* スケール/ピボット/回転はまずピボットを中心に合成し、position は
		* その後に適用する独立したワールド空間オフセット。ローカル
		* トランスフォームを更新するのはルートノード
		* (stages_[defaultStage_].nodes_)のみ — CumulateTransforms() が
		* 全子孫へ伝播する。ルートだけに変換全体を後乗せすれば、ローカル
		* トランスフォームの連鎖(node.globalTransform_ = local *
		* parentGlobal)を通じて正しく telescope するため。この Crister に
		* 変換対象の量子化済み頂点データが無い場合は false を返す。
		*/
		Bool ApplyTransformConversion(Vector3 position, Vector3 rotation, Vector3 scale, Vector3 pivot, const std::filesystem::path& cristerPath);

		/**
		* [EN]
		* Creates every GPU resource this Crister needs to draw: derives each
		* Cluster's streaming page ranges from its meshlet slice and pins the
		* pages that must never evict (coarsest cluster per SubMesh, skinned
		* LOD 0, the vertex pool they reference), uploads skin attributes for
		* the pool range, flattens each SubMesh's coarsest cluster into RT
		* proxy geometry (compressed vertices + decoded float3 positions +
		* flat 32-bit triangle indices, deduplicated), sets up per-texture
		* streaming state from BakeBitmap()'s BC7 mip chains (clamped to the
		* leading run of mip levels legal as a standalone single-mip BC7
		* resource), then registers with the shared streaming bookkeeping and
		* brings the pinned geometry pages/texture mips resident so the model
		* is immediately drawable at its fallback LOD/resolution. Finer
		* pages/mips stream in later on demand (MakeClusterResident/
		* MakeTextureMipResident, driven by ModelRenderer::Gather).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この Crister の描画に必要な全 GPU リソースを作成する: 各 Cluster の
		* ストリーミングページ範囲を meshlet スライスから導出し、追い出しては
		* いけないページ（SubMesh ごとの最粗クラスタ、スキンド LOD 0、それらが
		* 参照する頂点プール）をピン留めする。プール範囲分のスキニング属性を
		* アップロードし、各 SubMesh の最粗クラスタを RT プロキシジオメトリ
		* （圧縮頂点 + デコード済み float3 位置 + 重複排除済みフラット 32bit
		* 三角形インデックス）へ展開する。BakeBitmap() が焼いた BC7 ミップ
		* チェーンから（「単一ミップの独立した BC7 リソース」として合法な
		* 先頭のミップ範囲へ切り詰めた上で）テクスチャごとのストリーミング
		* 状態を準備し、最後に共有ストリーミング管理へ登録してピン留め
		* ジオメトリページ/テクスチャミップを常駐させ、フォールバック
		* LOD/解像度で即座に描画可能にする。より細かいページ/ミップは後で
		* オンデマンドにストリームインする（MakeClusterResident/
		* MakeTextureMipResident、ModelRenderer::Gather から駆動）。
		*/
		void Upload(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap);

		/**
		* [EN]
		* Returns every Stage (root-node list + name) parsed from the source
		* glTF's scenes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ソース glTF の scenes から解析された、全 Stage（ルートノード一覧 +
		* 名前）を返す。
		*/
		[[nodiscard]] const DynamicArray<Stage>& Stages()const;

		/**
		* [EN]
		* Returns every Node in the flattened node hierarchy, including their
		* local S/R/T and cumulated globalTransform_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 平坦化されたノード階層の全 Node を返す。各ノードのローカル
		* S/R/T と、累積済みの globalTransform_ を含む。
		*/
		[[nodiscard]] const DynamicArray<Node>& Nodes()const;

		/**
		* [EN]
		* Returns every KHR_lights_punctual point/spot light resolved from
		* the source glTF, in world space.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ソース glTF から解決された、全 KHR_lights_punctual
		* ポイント/スポットライトをワールド空間で返す。
		*/
		[[nodiscard]] const DynamicArray<PunctualLight>& Lights()const;

		/**
		* [EN]
		* Returns every SubMesh (material + cluster range + optional skin
		* index) this Crister is split into.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この Crister が分割されている全 SubMesh（マテリアル + クラスタ
		* 範囲 + 任意のスキンインデックス）を返す。
		*/
		[[nodiscard]] const DynamicArray<SubMesh>& SubMeshes()const;

		/**
		* [EN]
		* Returns every Material (PBR factors + KHR_materials_* extensions +
		* texture indices) parsed from the source glTF.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ソース glTF から解析された、全 Material（PBR ファクタ +
		* KHR_materials_* 拡張 + テクスチャインデックス）を返す。
		*/
		[[nodiscard]] const DynamicArray<Material>& Materials()const;

		/**
		* [EN]
		* Returns every Skin (joint list + inverse-bind matrices) parsed
		* from the source glTF.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ソース glTF から解析された、全 Skin（ジョイント一覧 + 逆バインド
		* 行列）を返す。
		*/
		[[nodiscard]] const DynamicArray<Skin>& Skins()const;

		/**
		* [EN]
		* Returns every Cluster (one LOD level's meshlet range within a
		* SubMesh) across all SubMeshes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全 SubMesh を通した、全 Cluster（SubMesh 内の1 LOD レベルぶんの
		* meshlet 範囲）を返す。
		*/
		[[nodiscard]] const DynamicArray<Cluster>& Clusters()const;

		/**
		* [EN]
		* Returns the index into Stages() of the stage rendered by default
		* when no stage is explicitly selected.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 明示的にステージが選択されていない時にデフォルトで描画される、
		* Stages() 内のインデックスを返す。
		*/
		[[nodiscard]] Int DefaultStage()const;

		/**
		* [EN]
		* Linear search for the first Node whose name_ matches name, or -1
		* if none does. glTF node names are not guaranteed unique, so this
		* returns the first match in nodes_ order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* name_ が name と一致する最初の Node を線形探索する、無ければ -1。
		* glTF のノード名は一意性が保証されないため、nodes_ の順で最初に
		* 見つかったものを返す。
		*/
		[[nodiscard]] Int FindNodeIndex(const std::string& name)const;

		/**
		* [EN]
		* Minimum corner of the dequantisation AABB for CompressedVertex
		* positions (see the struct comment). Passed to the shaders through
		* ModelInstance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CompressedVertex 位置の逆量子化 AABB の最小コーナー（構造体コメント
		* 参照）。ModelInstance 経由でシェーダに渡す。
		*/
		[[nodiscard]] Vector3 PositionMin()const;

		/**
		* [EN]
		* Extent (max - min) of the dequantisation AABB for CompressedVertex
		* positions. Passed to the shaders through ModelInstance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CompressedVertex 位置の逆量子化 AABB の大きさ（max - min）。
		* ModelInstance 経由でシェーダに渡す。
		*/
		[[nodiscard]] Vector3 PositionExtent()const;

		/**
		* [EN]
		* Minimum corner of the dequantisation AABB for CompressedVertex
		* texcoords. Passed to the shaders through ModelInstance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CompressedVertex テクスチャ座標の逆量子化 AABB の最小コーナー。
		* ModelInstance 経由でシェーダに渡す。
		*/
		[[nodiscard]] Vector2 TexcoordMin()const;

		/**
		* [EN]
		* Extent (max - min) of the dequantisation AABB for CompressedVertex
		* texcoords. Passed to the shaders through ModelInstance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CompressedVertex テクスチャ座標の逆量子化 AABB の大きさ
		* （max - min）。ModelInstance 経由でシェーダに渡す。
		*/
		[[nodiscard]] Vector2 TexcoordExtent()const;

		/**
		* [EN]
		* GPU address of the RT proxy's dedicated float3 position buffer
		* (stride = sizeof(Vector3)) for BLAS construction, decoded on the
		* CPU from the quantised CompressedVertex data with the same math
		* the mesh shader uses — so BLAS positions match the rasterized ones.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* BLAS 構築用の、RT プロキシ専用 float3 位置バッファ
		* （stride = sizeof(Vector3)）の GPU アドレス。量子化済み
		* CompressedVertex からメッシュシェーダと同じ計算で CPU デコード
		* するため、BLAS の位置はラスタライズ結果と一致する。
		*/
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS PositionBufferAddress()const;

		/**
		* [EN]
		* Vertex count of the RT proxy's compact position/vertex buffers
		* (positionResource_/vertexResource_) PositionBufferAddress() points
		* to, i.e. the size a morph-blend scratch position buffer must be
		* allocated to.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* PositionBufferAddress() が指す RT プロキシのコンパクトな位置/
		* 頂点バッファ(positionResource_/vertexResource_)の頂点数。
		* モーフブレンド用の一時位置バッファを確保すべきサイズでもある。
		*/
		[[nodiscard]] Uint32 VertexCount()const;

		/**
		* [EN]
		* GPU address of the flat (non-meshlet) 32-bit triangle index buffer
		* for BLAS construction, unpacked once at Upload() time from
		* primitiveIndices_/vertexIndices_ in the same order the mesh shader
		* draws them — so BLAS geometry matches the rasterized geometry
		* exactly (no re-derived winding).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* BLAS 構築用の、フラット（非meshlet）32bit 三角形インデックス
		* バッファの GPU アドレス。primitiveIndices_/vertexIndices_ から
		* Upload() 時に1度だけ、メッシュシェーダが描くのと同じ順序で展開する
		* — BLAS のジオメトリはラスタライズされるジオメトリと完全に一致する
		* （巻き順を再導出しない）。
		*/
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS IndexBufferAddress()const;

		/**
		* [EN]
		* Triangle count of the flat index buffer IndexBufferAddress() points to.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* IndexBufferAddress() が指すフラットインデックスバッファの三角形数。
		*/
		[[nodiscard]] Uint32 IndexCount()const;


		/**
		* [EN]
		* Maps a glTF image index (as stored in Material) to the bindless
		* heap index of the uploaded GPU texture. Returns 0xFFFFFFFF when
		* not present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* glTF の image インデックス（Material に格納されている値）を、
		* アップロード済み GPU テクスチャの bindless ヒープインデックスに
		* 変換する。無ければ 0xFFFFFFFF。
		*/
		[[nodiscard]] Uint TextureBindlessIndex(Uint32 textureIndex)const;

		/**
		* [EN]
		* Whether any texture's resident bindless index has changed (via
		* MakeTextureMipResident/EvictTextureMip) since the last
		* ClearMaterialsDirty() - RaytracingRenderer checks this to know
		* whether its cached reflection/GI material table (which baked
		* TextureBindlessIndex() at build time) needs rebuilding.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前回の ClearMaterialsDirty() 以降に、いずれかのテクスチャの常駐
		* バインドレスインデックスが(MakeTextureMipResident/EvictTextureMip
		* 経由で)変わったか。RaytracingRenderer がこれを見て、キャッシュ済みの
		* 反射/GI マテリアルテーブル(構築時に TextureBindlessIndex() を
		* 焼き込んでいる)を再構築すべきか判断する。
		*/
		[[nodiscard]] Bool IsMaterialsDirty()const;

		/**
		* [EN]
		* Clears the flag IsMaterialsDirty() reports, after the caller has
		* rebuilt whatever depended on the stale bindless indices.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* IsMaterialsDirty() が報告するフラグをクリアする。呼び出し側が、
		* 古いバインドレスインデックスに依存していたものを再構築した後に呼ぶ。
		*/
		void ClearMaterialsDirty()const;

		/**
		* [EN]
		* Bindless SRV of the RT proxy CompressedVertex buffer (built from
		* each SubMesh's coarsest cluster — reflections trace against a
		* low-poly proxy so full-detail geometry never has to be resident).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* RT プロキシ CompressedVertex バッファの bindless SRV（各 SubMesh の
		* 最粗クラスタから構築 — 反射は低ポリプロキシに対してトレースする
		* ため、フル詳細ジオメトリを常駐させる必要がない）。
		*/
		[[nodiscard]] Uint VertexBufferIndex()const;

		/**
		* [EN]
		* Bindless SRV over the flat 32-bit triangle index buffer, for
		* raytracing closesthit shaders to re-fetch the hit triangle's
		* vertices (PrimitiveIndex() * 3 + 0/1/2 -> vertex index).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* フラット 32bit 三角形インデックスバッファの bindless SRV。
		* レイトレの closesthit がヒット三角形の頂点を引き直すのに使う
		* (PrimitiveIndex() * 3 + 0/1/2 → 頂点インデックス)。
		*/
		[[nodiscard]] Uint IndexBufferIndex()const;

		/**
		* [EN]
		* Bindless SRV of the CompressedSkinVertex buffer, or 0xFFFFFFFF
		* when this Crister has no skins.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CompressedSkinVertex バッファの bindless SRV。スキンが無い
		* Crister では 0xFFFFFFFF。
		*/
		[[nodiscard]] Uint SkinVertexBufferIndex()const;

		/**
		* [EN]
		* Bindless SRV of morphDeltaResource_ (raster-side flat morph
		* target delta pool, target-major per SubMesh — see
		* SubMesh::morphDeltaOffset_), or 0xFFFFFFFF when no SubMesh has
		* morphs_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* morphDeltaResource_(ラスタ側のフラットなモーフターゲットデルタ
		* プール、SubMesh ごとのターゲット主順 — SubMesh::morphDeltaOffset_
		* 参照)の bindless SRV。どの SubMesh も morphs_ を持たなければ
		* 0xFFFFFFFF。
		*/
		[[nodiscard]] Uint MorphDeltaBufferIndex()const;

		/**
		* [EN]
		* Bindless SRV of vertexMorphSourceResource_ (per-vertex remap to
		* the original vertex morphDeltaResource_ is indexed by — see
		* vertexMorphSource_'s comment), or 0xFFFFFFFF when no SubMesh has
		* morphs_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* vertexMorphSourceResource_(morphDeltaResource_ がインデックスする
		* オリジナル頂点への、頂点ごとの逆引き — vertexMorphSource_ の
		* コメント参照)の bindless SRV。どの SubMesh も morphs_ を持たなければ
		* 0xFFFFFFFF。
		*/
		[[nodiscard]] Uint VertexMorphSourceBufferIndex()const;

		/**
		* [EN]
		* Whether this Crister has RT-side skinning data (skinVertexResource_/
		* raytracingSkinVertexResource_ populated), i.e. skins_ is non-empty and the
		* RT proxy build found at least one skinned SubMesh.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この Crister が RT 側のスキニングデータを持つか
		* (skinVertexResource_/raytracingSkinVertexResource_ が構築済みか)。
		* skins_ が空でなく、RT プロキシ構築時にスキンド SubMesh を
		* 1つ以上見つけた場合に true。
		*/
		[[nodiscard]] Bool IsProxySkinned()const;

		/**
		* [EN]
		* GPU address of the RT proxy's skin vertex pool
		* (raytracingSkinVertexResource_), or 0 when IsProxySkinned is false.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* RT プロキシのスキン頂点プール (raytracingSkinVertexResource_) の
		* GPU アドレス。IsProxySkinned が false なら 0。
		*/
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS ProxySkinVertexBufferAddress()const;

		/**
		* [EN]
		* GPU address of the RT proxy's morph delta pool
		* (raytracingMorphDeltaResource_), or 0 when no SubMesh has morphs_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* RT プロキシのモーフデルタプール (raytracingMorphDeltaResource_) の
		* GPU アドレス。どの SubMesh も morphs_ を持たなければ 0。
		*/
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS ProxyMorphDeltaBufferAddress()const;

		/**
		* [EN]
		* Copies the RT proxy's base (bind-pose) positions
		* (positionResource_, VertexCount() * sizeof(Vector3) bytes) into
		* destination, which the caller must have already transitioned to
		* D3D12_RESOURCE_STATE_COPY_DEST. Used to seed a per-instance morph
		* blend scratch buffer before MorphBlendCS overwrites only the
		* vertex ranges of SubMeshes with active morph weights this frame —
		* every other vertex must keep its base position unchanged.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* RT プロキシのベース(バインドポーズ)位置(positionResource_、
		* VertexCount() * sizeof(Vector3) バイト)を destination へコピー
		* する。呼び出し側は destination を事前に
		* D3D12_RESOURCE_STATE_COPY_DEST へ遷移させておくこと。今フレーム
		* 有効なモーフウェイトを持つ SubMesh の頂点範囲だけを MorphBlendCS が
		* 上書きする前の、インスタンスごとのモーフブレンド用一時バッファの
		* 種として使う — それ以外の頂点はベース位置のまま保つ必要がある。
		*/
		void CopyMorph(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* destination)const;

		/**
		* [EN]
		* Geometry streaming (Nanite-style residency) + budget control
		* overview. A page is one Cluster's slice of meshlets/bounds/vertex-
		* indices/primitive-indices (+ its own vertex range for LOD>=1). LOD 0
		* clusters share the original vertex pool page instead. Pages upload
		* on demand from the CPU-resident arrays and are evicted by
		* EvictClusterBudget when total VRAM exceeds the budget. Pinned pages
		* (coarsest cluster per SubMesh, skinned LOD 0 and the pool they
		* reference) never evict, so every model can always be drawn at some
		* LOD. MakeClusterResident/MakePoolResident/EvictCluster/EvictPool are
		* the resident/evict pair for cluster vs. pool. Public only because
		* MakeClusterResident is also driven externally by ModelRenderer; the
		* naming/shape of this whole group still needs a pass — flagged in
		* review, not yet fixed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ジオメトリストリーミング（Nanite 型の常駐管理）+ 予算制御の概要。
		* ページは 1 Cluster 分の meshlet/バウンド/頂点インデックス/
		* プリミティブインデックスのスライス（LOD>=1 は専用頂点範囲込み）。
		* LOD 0 クラスタは代わりに元の共有頂点プールページを参照する。
		* ページは CPU 常駐配列からオンデマンドでアップロードされ、VRAM が
		* 予算を超えると EvictClusterBudget が追い出す。ピン留めページ（SubMesh
		* ごとの最粗クラスタ、スキンド LOD 0 とそれが参照するプール）は
		* 追い出さないため、モデルは常に何らかの LOD で描画できる。
		* MakeClusterResident/MakePoolResident/EvictCluster/EvictPool は
		* クラスタ/プールの常駐・追い出しの対。MakeClusterResident が
		* ModelRenderer からも呼ばれるため public。この群の命名・形は
		* レビューで指摘済み、未修正。
		*/

		/**
		* [EN]
		* Synchronously uploads the cluster's page (and the vertex pool if
		* the cluster is LOD 0). No-op when already resident.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* クラスタのページを同期アップロードする（LOD 0 なら頂点プールも）。
		* 常駐済みなら何もしない。
		*/
		void MakeClusterResident(Uint32 clusterIndex);

		/**
		* [EN]
		* Texture mip counterpart to MakeClusterResident: streams a mip level
		* instead of a cluster (see StreamingTexture below). targetMip is
		* uploaded directly — NOT one level at a time — so a texture reaches
		* the resolution the camera wants in a single upload instead of
		* converging over as many frames as there are mip levels (which left
		* visible blur whenever the camera moved, since each step also
		* blocks on a GPU fence).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* MakeClusterResident のテクスチャミップ版の対。クラスタではなく
		* ミップレベルをストリーミングする（下記 StreamingTexture 参照）。
		* targetMip は1段ずつではなく直接アップロードする — カメラが要求する
		* 解像度へ1回のアップロードで到達させるため。1段ずつだとミップ段数分の
		* フレームを要し（各段が GPU フェンス待ちで同期もする）、カメラを
		* 動かすたびにボケが残っていた。
		*/
		void MakeTextureMipResident(Uint32 textureIndex, Uint32 targetMip);

		/**
		* [EN]
		* Synchronously uploads the shared LOD 0 vertex pool page. No-op when
		* already resident, when poolVertexEnd_ is 0 (no LOD 0 clusters), or
		* when no device is bound yet.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 共有 LOD 0 頂点プールページを同期アップロードする。常駐済み、
		* poolVertexEnd_ が 0（LOD 0 クラスタが無い）、またはまだ device が
		* 束縛されていない場合は何もしない。
		*/
		void MakePoolResident();

		/**
		* [EN]
		* Frees the cluster page's GPU resources and bindless indices
		* (deferred release, since in-flight frames may still reference
		* them). No-op if not resident or pinned.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* クラスタページの GPU リソースと bindless インデックスを解放する
		* （インフライトのフレームがまだ参照している可能性があるため遅延
		* 解放）。常駐していないかピン留め済みなら何もしない。
		*/
		void EvictCluster(Uint32 clusterIndex);

		/**
		* [EN]
		* Frees the texture's current (non-pinned) mip resource, falling
		* back to the pinned coarsest mip. No-op if not valid or already at
		* the pinned mip.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* テクスチャの現在の（ピン留めでない）ミップリソースを解放し、
		* ピン留めされた最粗ミップへ戻す。無効、または既にピン留めミップの
		* 場合は何もしない。
		*/
		void EvictTextureMip(Uint32 textureIndex);

		/**
		* [EN]
		* Frees the shared LOD 0 vertex pool page's GPU resource. No-op if
		* not resident, pinned, or still referenced by a resident cluster
		* page (poolResidentReferences_ > 0).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 共有 LOD 0 頂点プールページの GPU リソースを解放する。常駐していない、
		* ピン留め済み、または常駐中のクラスタページから参照されている場合
		* （poolResidentReferences_ > 0）は何もしない。
		*/
		void EvictPool();

		/**
		* [EN]
		* Marks the cluster page (and its pool, if the cluster doesn't own
		* its own vertices) as used this frame so the eviction age guard
		* keeps it alive while the GPU may still reference it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* クラスタページ（頂点を自前で持たないクラスタならプールも）を
		* 今フレーム使用中として記録し、GPU がまだ参照しうる間は追い出しの
		* 経過フレームガードで生存させる。
		*/
		void TouchCluster(Uint32 clusterIndex, Uint64 frame);

		/**
		* [EN]
		* Marks the texture's currently-resident mip as used this frame so
		* the eviction age guard keeps it alive while the GPU may still
		* reference it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* テクスチャの現在常駐中のミップを今フレーム使用中として記録し、
		* GPU がまだ参照しうる間は追い出しの経過フレームガードで生存させる。
		*/
		void TouchTexture(Uint32 textureIndex, Uint64 frame);

		/**
		* [EN]
		* Whether the cluster's page is currently GPU-resident.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* クラスタのページが現在 GPU に常駐しているか。
		*/
		[[nodiscard]] Bool IsClusterResident(Uint32 clusterIndex)const;

		/**
		* [EN]
		* Whether the texture has streamed all the way in to mip 0 (its finest).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* テクスチャがミップ 0（最も細かい）まで完全にストリームインしているか。
		*/
		[[nodiscard]] Bool IsTextureResident(Uint32 textureIndex)const;

		/**
		* [EN]
		* Evicts unpinned cluster pages (oldest first) across every live
		* Crister, then unpinned/unreferenced vertex pools, until total
		* resident geometry fits geometryBudgetBytes_. Pages must be unused
		* for evictAgeFrames_ frames before eviction so in-flight GPU work
		* never loses a buffer it references.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 生存中の全 Crister を対象に、未ピンのクラスタページを古い順に、
		* 続いて未ピン/未参照の頂点プールを追い出し、常駐ジオメトリ合計を
		* geometryBudgetBytes_ 以内に収める。インフライトの GPU 作業が
		* 参照中のバッファを失わないよう、evictAgeFrames_ フレーム未使用の
		* ページのみ追い出す。
		*/
		static void EvictClusterBudget(Uint64 currentFrame);

		/**
		* [EN]
		* Texture counterpart to EvictClusterBudget: evicts unpinned
		* resident texture mips (oldest first) across every live Crister
		* until total resident texture memory fits textureBudgetBytes_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* EvictClusterBudget のテクスチャ版の対: 生存中の全 Crister を対象に、
		* 未ピンの常駐ミップを古い順に追い出し、常駐テクスチャメモリ合計を
		* textureBudgetBytes_ 以内に収める。
		*/
		static void EvictTextureBudget(Uint64 currentFrame);

		/**
		* [EN]
		* Bindless SRV of the cluster page's vertex buffer — its own page
		* if it owns vertices, otherwise the shared LOD 0 pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* クラスタページの頂点バッファの bindless SRV — 頂点を自前で持てば
		* 自身のページ、そうでなければ共有 LOD 0 プール。
		*/
		[[nodiscard]] Uint ClusterVertexBufferIndex(Uint32 clusterIndex)const;

		/**
		* [EN]
		* Bindless SRV of the cluster page's meshlet buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* クラスタページの meshlet バッファの bindless SRV。
		*/
		[[nodiscard]] Uint ClusterMeshletBufferIndex(Uint32 clusterIndex)const;

		/**
		* [EN]
		* Bindless SRV of the cluster page's meshlet bound buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* クラスタページの meshlet バウンドバッファの bindless SRV。
		*/
		[[nodiscard]] Uint ClusterMeshletBoundBufferIndex(Uint32 clusterIndex)const;

		/**
		* [EN]
		* Bindless SRV of the cluster page's vertex indices buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* クラスタページの頂点インデックスバッファの bindless SRV。
		*/
		[[nodiscard]] Uint ClusterVertexIndicesBufferIndex(Uint32 clusterIndex)const;

		/**
		* [EN]
		* Bindless SRV of the cluster page's primitive indices buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* クラスタページのプリミティブインデックスバッファの bindless SRV。
		*/
		[[nodiscard]] Uint ClusterPrimitiveIndicesBufferIndex(Uint32 clusterIndex)const;

		/**
		* [EN]
		* Whether this cluster page owns its own vertex slice
		* (streamingGeometry_[clusterIndex].ownsVertices_) rather than
		* referencing the shared LOD 0 pool. Own-page vertex indices are
		* rebased to page-local numbering by MakeClusterResident, so they
		* are NOT valid indices into Crister::vertexMorphSource_/
		* morphDeltaResource_ (which use the crister-wide numbering the
		* shared pool preserves) — callers populating a raster morph
		* instance must check this and leave morph fields zeroed
		* (ModelInstanceData::morphTargetCount_ == 0) for any cluster where
		* this returns true.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このクラスタページが共有 LOD 0 プールを参照するのではなく、
		* 自前の頂点スライスを持つか
		* (streamingGeometry_[clusterIndex].ownsVertices_)。自前ページの
		* 頂点インデックスは MakeClusterResident によってページローカルな
		* 番号へリベースされるため、Crister::vertexMorphSource_/
		* morphDeltaResource_(共有プールが保つ Crister 全体の番号付けを使う)
		* への有効なインデックスでは【ない】— ラスタのモーフ用インスタンス
		* を組み立てる側はこれを確認し、true が返るクラスタでは
		* モーフフィールドをゼロのまま
		* (ModelInstanceData::morphTargetCount_ == 0)にすること。
		*/
		[[nodiscard]] Bool StandaloneVertices(Uint32 clusterIndex)const;

		/**
		* [EN]
		* Finest mip level currently GPU-resident, or 0 if textureIndex is
		* out of range.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在 GPU に常駐している最も細かいミップ段。textureIndex が範囲外
		* なら 0。
		*/
		[[nodiscard]] Uint32 TextureFinestMip(Uint32 textureIndex)const;

		/**
		* [EN]
		* Approximates the mip a material texture needs from the same
		* worldScale/pixelsPerUnit metric ModelRenderer already computes for
		* cluster LOD selection (screen coverage of the instance).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ModelRenderer がクラスタ LOD 選択のために既に計算している
		* worldScale/pixelsPerUnit（インスタンスの画面被覆率）から、
		* マテリアルテクスチャに必要なミップを近似する。
		*/
		[[nodiscard]] Uint32 TextureDesiredMip(Uint32 textureIndex, Float worldScale, Float pixelsPerUnit)const;

	private:
		/**
		* [EN]
		* Walks stages_[defaultStage_]'s node tree depth-first and
		* recomputes every Node::globalTransform_ from local S/R/T.
		* Duplicates ModelLoader::CumulateTransforms' logic so
		* ApplyAxisConversion can recompute global transforms without a
		* ModelLoader/glTF re-parse.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* stages_[defaultStage_]のノードツリーを深さ優先で走査し、ローカル
		* S/R/T から全 Node::globalTransform_ を再計算する。
		* ModelLoader::CumulateTransforms のロジックを複製したもの。
		* ApplyAxisConversion が ModelLoader/glTF 再解析無しにグローバル
		* トランスフォームを再計算できるようにする。
		*/
		void CumulateTransforms();

		/**
		* [EN]
		* Allocates a bindless heap index and creates a StructuredBuffer SRV
		* for resource at that index.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* bindless ヒープインデックスを1つ確保し、resource に対する
		* StructuredBuffer SRV をそのインデックスへ作成する。
		*/
		static Uint CreateStructuredShaderResourceView(ID3D12Device* device, BindlessHeap* heap, ID3D12Resource* resource, Uint elementCount, Uint stride);

		/**
		* [EN]
		* DirectX::CreateStaticBuffer rejects any resource above ~128MB
		* (D3D12_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM used
		* directly as a flat byte cap by DirectXTK12's BufferHelpers.cpp,
		* not an actual D3D12 hardware limit - real buffers can be
		* gigabytes, bounded only by available GPU memory). High-poly
		* meshes routinely exceed that for the flat 32-bit triangle index
		* buffer, so this mirrors CreateStaticBuffer's own implementation
		* without the artificial size gate.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* DirectX::CreateStaticBuffer は約128MBを超えるリソースを拒否する
		* (DirectXTK12 の BufferHelpers.cpp が
		* D3D12_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM を単純な
		* バイト上限としてそのまま使っているだけで、実際の D3D12/GPU の
		* ハード制限ではない - 実バッファは GPU メモリが許す限り
		* ギガバイト単位まで作れる)。ハイポリメッシュのフラット32bit
		* 三角形インデックスバッファはこれを普通に超えるため、
		* CreateStaticBuffer と同じ実装からサイズ上限チェックだけ外した版。
		*/
		static HRESULT CreateStaticBufferUnbounded(ID3D12Device* device, DirectX::ResourceUploadBatch& resourceUpload, const void* data, Uint64 count, Uint64 stride, D3D12_RESOURCE_STATES afterState, ID3D12Resource** outResource);

		/**
		* [EN]
		* Texture counterpart to CreateStaticBufferUnbounded's byte-layout
		* role: resolves mip dimensions/row-pitch/slice-pitch/byte-offset
		* within Bitmap::cacheData_ for a given mip index, from the standard
		* BC block layout (16 bytes per 4x4 texel block, mips concatenated
		* in order with no stored per-mip offsets).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CreateStaticBufferUnbounded のバイトレイアウト計算に相当する
		* テクスチャ版: Bitmap::cacheData_ 内での指定ミップの寸法/
		* row-pitch/slice-pitch/バイトオフセットを、標準的な BC
		* ブロックレイアウト（4x4 テクセルブロックあたり 16 バイト、
		* ミップは順に連結・オフセット未保存）から解決する。
		*/
		static void ComputeTextureMipLayout(const Bitmap& bitmap, Uint32 mipIndex, Uint32& outWidth, Uint32& outHeight, Uint64& outRowPitch, Uint64& outSlicePitch, Uint64& outByteOffset);

		/**
		* [EN]
		* Dequantises just the position field of a CompressedVertex, against
		* this Crister's own positionMin_/positionExtent_. A separate function
		* from DecodeVertex because some callers only need position (deduping
		* by position, or building a position-only buffer) and would otherwise
		* pay for decoding normal/tangent/texcoord they never use.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CompressedVertex の position フィールドだけを、この Crister 自身の
		* positionMin_/positionExtent_ に対して逆量子化する。DecodeVertex とは
		* 別関数にしてあるのは、呼び出し元の一部が position だけを必要とする
		* （position で重複排除する、position だけのバッファを作る）ため —
		* そうしないと使わない normal/tangent/texcoord のデコードまで払うことになる。
		*/
		[[nodiscard]] Vector3 DecodePosition(const CompressedVertex& compressed)const;

		/**
		* [EN]
		* Dequantises just the texcoord field of a CompressedVertex, against
		* this Crister's own texcoordMin_/texcoordExtent_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CompressedVertex の texcoord フィールドだけを、この Crister 自身の
		* texcoordMin_/texcoordExtent_ に対して逆量子化する。
		*/
		[[nodiscard]] Vector2 DecodeTexcoord(const CompressedVertex& compressed)const;

		/**
		* [EN]
		* Dequantises just the tangent field (xyz direction + handedness sign
		* in .w) of a CompressedVertex.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CompressedVertex の tangent フィールド（xyz 方向 + .w の利き手符号）
		* だけを逆量子化する。
		*/
		[[nodiscard]] Vector4 DecodeTangent(const CompressedVertex& compressed)const;

		/**
		* [EN]
		* Dequantises the joint indices and renormalised weights of a
		* CompressedSkinVertex. Weights are renormalised because four 8-bit
		* UNORM values rarely sum to exactly one.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CompressedSkinVertex のジョイントインデックスと再正規化済み
		* ウェイトを逆量子化する。4つの 8bit UNORM 値は合計がちょうど1に
		* なることが稀なため再正規化する。
		*/
		void DecodeSkin(const CompressedSkinVertex& compressed, XmUint4& outJoints, Vector4& outWeights)const;

		/**
		* [EN]
		* Change-of-basis for a position: transforms vector in place by basis.
		* Duplicates ModelLoader's ConvertPositionByBasis (private to
		* ModelLoader) so ApplyAxisConversion can re-convert without a
		* ModelLoader/glTF re-parse.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 位置の基底変換。vector を basis でその場変換する。ModelLoader の
		* ConvertPositionByBasis（ModelLoader 限定 private）を複製し、
		* ApplyAxisConversion が ModelLoader/glTF 再解析無しに再変換できる
		* ようにする。
		*/
		void ConvertPositionByBasis(Vector3& vector, const Matrix& basis)const;

		/**
		* [EN]
		* Change-of-basis for a rotation: transforms quaternion in place by basis.
		* Duplicates ModelLoader's ConvertRotationByBasis (private to ModelLoader).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 回転の基底変換。quaternion を basis でその場変換する。ModelLoader の
		* ConvertRotationByBasis（ModelLoader 限定 private）を複製する。
		*/
		void ConvertRotationByBasis(Quaternion& quaternion, const Matrix& basis)const;

		/**
		* [EN]
		* Change-of-basis for a matrix: transforms matrix in place by basis.
		* Duplicates ModelLoader's ConvertMatrixByBasis (private to ModelLoader).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 行列の基底変換。matrix を basis でその場変換する。ModelLoader の
		* ConvertMatrixByBasis（ModelLoader 限定 private）を複製する。
		*/
		void ConvertMatrixByBasis(Matrix& matrix, const Matrix& basis)const;

		/**
		* [EN]
		* Quantises a [0,1] float to a 16-bit UNORM, clamping out-of-range
		* input first. Shared by EncodeVertex (position/texcoord) and
		* EncodeOctahedralNormal.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [0,1] の float を 16bit UNORM へ量子化する。範囲外の入力は先に
		* クランプする。EncodeVertex（position/texcoord）と
		* EncodeOctahedralNormal が共有する。
		*/
		[[nodiscard]] static Uint32 QuantizeUnorm16(Float value01);

		/**
		* [EN]
		* Quantises a [0,1] float to an 8-bit UNORM, clamping out-of-range
		* input first. Used by BakeMesh for skin weights.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [0,1] の float を 8bit UNORM へ量子化する。範囲外の入力は先に
		* クランプする。BakeMesh がスキンウェイトに使う。
		*/
		[[nodiscard]] static Uint32 QuantizeUnorm8(Float value01);

		/**
		* [EN]
		* One streamable GPU page: a Cluster's rebased slice of geometry.
		* Offsets stored in the page's meshlets are page-local; LOD>=1
		* pages own their vertex slice, LOD 0 pages reference the shared
		* vertex pool page instead (vertexBufferIndex_ = pool SRV).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ストリーミング可能な GPU ページ 1 つ = Cluster のリベース済み
		* ジオメトリスライス。ページ内 meshlet のオフセットはページ
		* ローカル。LOD>=1 は専用頂点スライスを所有し、LOD 0 は共有頂点
		* プールページを参照する（vertexBufferIndex_ = プール SRV）。
		*/
		struct StreamingGeometry
		{
			/// [JP] CPU 配列内の派生レンジ（ロード時に meshlet 列から導出）。
			Uint32 vertexIndexBegin_ = 0;
			Uint32 vertexIndexEnd_ = 0;
			Uint32 primitiveBegin_ = 0;
			Uint32 primitiveEnd_ = 0;
			Uint32 vertexBegin_ = 0;
			Uint32 vertexEnd_ = 0;
			Bool ownsVertices_ = false;
			Bool pinned_ = false;

			Microsoft::WRL::ComPtr<ID3D12Resource> meshletResource_;
			Microsoft::WRL::ComPtr<ID3D12Resource> meshletBoundResource_;
			Microsoft::WRL::ComPtr<ID3D12Resource> vertexIndicesResource_;
			Microsoft::WRL::ComPtr<ID3D12Resource> primitiveIndicesResource_;
			Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;

			Uint meshletBufferIndex_ = 0xFFFFFFFF;
			Uint meshletBoundBufferIndex_ = 0xFFFFFFFF;
			Uint vertexIndicesBufferIndex_ = 0xFFFFFFFF;
			Uint primitiveIndicesBufferIndex_ = 0xFFFFFFFF;
			Uint vertexBufferIndex_ = 0xFFFFFFFF;

			Bool resident_ = false;
			Uint64 sizeBytes_ = 0;
			Uint64 lastUsedFrame_ = 0;
		};

		/**
		* [EN]
		* One streamable GPU mip of a Model-embedded texture: a single-mip
		* committed resource at that mip's own resolution. UV space is the
		* same regardless of which mip currently backs the SRV, so swapping
		* it in/out needs no shader-side change.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Model 内蔵テクスチャのストリーミング可能な GPU ミップ 1 つ。
		* そのミップ自身の解像度を持つ単一ミップの committed resource。
		* どのミップが SRV の裏にあっても UV 空間は変わらないため、
		* 出し入れにシェーダ側の変更は不要。
		*/
		struct TextureMipLevel
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
			Uint bindlessIndex_ = 0xFFFFFFFF;
			Uint64 sizeBytes_ = 0;
		};

		/**
		* [EN]
		* Streaming state for one Bitmap. At most 2 mips are ever GPU-resident
		* at once: pinnedMip_ (mipCount_-1, the coarsest, uploaded once at
		* Upload() and never freed — guarantees a fallback SRV) and
		* currentMip_ (topResidentMip_, the best mip streamed in so far;
		* unused while topResidentMip_ == mipCount_-1, i.e. nothing finer
		* than pinned has been requested yet). TextureBindlessIndex always
		* returns whichever of the two is finest, so intermediate mips
		* never need to stay resident — only the currently sampled one.
		* (Keeping the whole [topResidentMip_, mipCount_-1] range resident,
		* as an earlier version of this did, multiplies bindless-heap and
		* VRAM usage by up to mipCount_ per texture once TextureDesiredMip
		* legitimately converges toward mip 0 — exhausted the shared
		* BindlessHeap and corrupted other textures' descriptors. Do not
		* go back to that shape without also capping resident mips.)
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Bitmap 1 枚分のストリーミング状態。GPU 常駐は常に最大2ミップまで:
		* pinnedMip_(mipCount_-1、最粗。Upload() で一度だけアップロードし
		* 以後解放しない — フォールバック SRV を保証する)と currentMip_
		* (topResidentMip_、これまでにストリームインした最良ミップ。
		* topResidentMip_ == mipCount_-1 の間、つまりピン留めより細かい
		* ミップを未要求の間は未使用)。TextureBindlessIndex は常にこの2つの
		* うち細かい方を返すため、中間ミップを常駐させ続ける必要はなく、
		* 実際にサンプルされる1枚だけで済む。
		* (以前のバージョンのように [topResidentMip_, mipCount_-1] を丸ごと
		* 常駐させ続けると、TextureDesiredMip が正しくミップ0へ収束する
		* ようになった途端、1テクスチャあたり最大 mipCount_ 個分の
		* bindless ヒープ/VRAM を消費し、共有 BindlessHeap を枯渇させて
		* 他テクスチャのディスクリプタまで壊れた。常駐ミップ数を制限せずに
		* その形へ戻さないこと。)
		*/
		struct StreamingTexture
		{
			TextureMipLevel pinnedMip_;
			TextureMipLevel currentMip_;
			Uint32 mipCount_ = 0;
			Uint32 topResidentMip_ = 0;
			Uint64 lastUsedFrame_ = 0;
			Bool valid_ = false;
		};

		/// [EN] RT proxy geometry (coarsest cluster per SubMesh): compressed
		///      vertices for the hit shader, decoded float3 positions for BLAS,
		///      flat 32-bit triangle indices for both. Always resident, small.
		///      skinVertexResource_ covers the vertex pool range for skinned
		///      models (skinned SubMeshes are LOD 0 / pool-indexed only).
		/// [JP] RT プロキシジオメトリ（SubMesh ごとの最粗クラスタ）: ヒット
		///      シェーダ用圧縮頂点、BLAS 用デコード済み float3 位置、両者用の
		///      フラット 32bit 三角形インデックス。常に常駐・小サイズ。
		///      skinVertexResource_ はスキンドモデル用に頂点プール範囲を
		///      カバーする（スキンド SubMesh は LOD 0 / プール参照のみ）。
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> positionResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> skinVertexResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

		Microsoft::WRL::ComPtr<ID3D12Resource> raytracingSkinVertexResource_;

		/// [EN] Flat float3 morph-target delta pool for the RT proxy, target-
		///      major within each SubMesh's compact vertex range (see
		///      SubMesh::raytracingMorphDeltaOffset_/raytracingVertexOffset_/raytracingVertexCount_).
		///      Only allocated when at least one SubMesh has morphs_.
		/// [JP] RT プロキシ用のフラットな float3 モーフターゲットデルタ
		///      プール。各 SubMesh のコンパクト頂点範囲内でターゲット主順
		///      (SubMesh::raytracingMorphDeltaOffset_/raytracingVertexOffset_/
		///      raytracingVertexCount_ 参照)。いずれかの SubMesh が morphs_ を
		///      持つ場合のみ確保される。
		Microsoft::WRL::ComPtr<ID3D12Resource> raytracingMorphDeltaResource_;

		/// [EN] Raster-side morph support, bindless-registered (pulled via
		///      ResourceDescriptorHeap[...] from SkeletalModelMS.hlsl/
		///      StaticModelMS.hlsl, unlike the RT buffers above which are
		///      bound as root SRVs). morphDeltaResource_ is target-major
		///      per SubMesh (SubMesh::morphDeltaOffset_), baked straight
		///      from Morph::positionDeltas_ with no RT-proxy compaction.
		///      vertexMorphSourceResource_ mirrors vertices_/
		///      compressedVertices_ 1:1 (baked from vertexMorphSource_) so
		///      any streamed LOD's vertex can resolve back to the original
		///      vertex morphDeltaResource_ is indexed by (see
		///      vertexMorphSource_'s comment). Both allocated only when at
		///      least one SubMesh has morphs_.
		/// [JP] ラスタ側のモーフ対応。bindless 登録される(上の RT 用
		///      バッファがルート SRV で束縛されるのと違い、
		///      SkeletalModelMS.hlsl/StaticModelMS.hlsl から
		///      ResourceDescriptorHeap[...] 経由で引く)。
		///      morphDeltaResource_ は SubMesh ごとのターゲット主順
		///      (SubMesh::morphDeltaOffset_)で、Morph::positionDeltas_
		///      から RT プロキシの圧縮を経ずそのまま焼き込む。
		///      vertexMorphSourceResource_ は vertices_/compressedVertices_
		///      と 1:1 対応する(vertexMorphSource_ から焼き込む) —
		///      ストリームされたどの LOD の頂点も、morphDeltaResource_ が
		///      インデックスするオリジナル頂点へ逆引きできるようにする
		///      (vertexMorphSource_ のコメント参照)。いずれも、いずれかの
		///      SubMesh が morphs_ を持つ場合のみ確保される。
		Microsoft::WRL::ComPtr<ID3D12Resource> morphDeltaResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexMorphSourceResource_;
		Uint morphDeltaBufferIndex_ = 0xFFFFFFFF;
		Uint vertexMorphSourceBufferIndex_ = 0xFFFFFFFF;

		Uint vertexBufferIndex_ = 0xFFFFFFFF;
		Uint skinVertexBufferIndex_ = 0xFFFFFFFF;
		Uint indexBufferIndex_ = 0xFFFFFFFF;
		Uint32 triangleIndexCount_ = 0;
		Uint32 proxyVertexCount_ = 0;

		/// [EN] Logs the RT proxy's non-finite-position fallback once per
		///      Crister instead of once per degenerate vertex - see Upload()'s
		///      RT proxy geometry loop.
		/// [JP] RT プロキシの非有限位置フォールバックを、退化頂点ごとでなく
		///      Crister ごとに 1 度だけログする — Upload() の RT プロキシ
		///      ジオメトリループ参照。
		Bool degenerateRaytracingPositionLogged_ = false;

		/// [EN] Shared LOD 0 vertex pool page (referenced by LOD 0 cluster pages).
		/// [JP] 共有 LOD 0 頂点プールページ（LOD 0 クラスタページが参照する）。
		Microsoft::WRL::ComPtr<ID3D12Resource> poolResource_;
		Uint poolBufferIndex_ = 0xFFFFFFFF;
		Uint32 poolVertexEnd_ = 0;
		Uint32 poolResidentReferences_ = 0;
		Bool poolResident_ = false;
		Bool poolPinned_ = false;
		Uint64 poolSizeBytes_ = 0;
		Uint64 poolLastUsedFrame_ = 0;

		DynamicArray<StreamingGeometry> streamingGeometry_;
		DynamicArray<StreamingTexture> streamingTextures_;

		/// [EN] Set whenever any StreamingTexture's resident bindless index
		///      changes (MakeTextureMipResident/EvictTextureMip) - the
		///      reflection/GI material table (RaytracingRenderer::
		///      BuildReflectionMaterialTable) bakes TextureBindlessIndex()
		///      once and caches it, so a later mip swap leaves that baked
		///      index pointing at whatever the freed slot gets reused for
		///      next, or at nothing at all. ConsumeMaterialsDirty() lets the
		///      renderer notice and rebuild the table instead of reading
		///      through a stale/destroyed descriptor forever.
		/// [JP] StreamingTexture の常駐バインドレスインデックスが変わるたびに
		///      (MakeTextureMipResident/EvictTextureMip)立てる - 反射/GI
		///      マテリアルテーブル(RaytracingRenderer::
		///      BuildReflectionMaterialTable)は TextureBindlessIndex() を
		///      一度だけ焼き込んでキャッシュするため、後のミップ入れ替えは
		///      焼き込み済みインデックスを、解放されたスロットが次に再利用
		///      された先(あるいは何も無い先)を指したままにしてしまう。
		///      ConsumeMaterialsDirty() により、レンダラーがそれに気付いて
		///      テーブルを再構築できるようにする — 破棄済み/不正な
		///      ディスクリプタを読み続けさせない。
		mutable Bool materialsDirty_ = false;

		/// [EN] Captured at Upload() so pages can stream in and out later.
		///      These engine objects outlive every Crister.
		/// [JP] 後からページを出し入れできるよう Upload() で保持する。
		///      これらのエンジンオブジェクトは全 Crister より長寿命。
		ID3D12Device* device_ = nullptr;
		D3D12CommandQueue* uploadQueue_ = nullptr;
		BindlessHeap* bindlessHeap_ = nullptr;

		/// [EN] Dequantisation AABBs computed over all vertices at Upload() time.
		/// [JP] Upload() 時に全頂点から計算する逆量子化 AABB。
		Vector3 positionMin_ = { 0,0,0 };
		Vector3 positionExtent_ = { 1,1,1 };
		Vector2 texcoordMin_ = { 0,0 };
		Vector2 texcoordExtent_ = { 1,1 };

		/// [EN] Streaming bookkeeping shared by every live Crister. All access
		///      happens on the render thread. Geometry and textures track
		///      separate resident-byte totals against separate budgets, but
		///      share the registry and the eviction age guard.
		/// [JP] 生存中の全 Crister で共有するストリーミング管理。アクセスは
		///      すべてレンダースレッド上。ジオメトリとテクスチャは常駐バイト数と
		///      予算をそれぞれ別管理するが、レジストリと追い出しの経過フレーム
		///      ガードは共有する。
		static inline DynamicArray<Crister*> streamingRegistry_;

		static inline Uint64 totalResidentGeometryBytes_ = 0;
		static inline Uint64 totalResidentTextureBytes_ = 0;

		static inline Uint64 geometryBudgetBytes_ = 512ull * 1024 * 1024;
		static inline Uint64 textureBudgetBytes_ = 256ull * 1024 * 1024;

		static constexpr Uint64 evictAgeFrames_ = 8;
	};
}