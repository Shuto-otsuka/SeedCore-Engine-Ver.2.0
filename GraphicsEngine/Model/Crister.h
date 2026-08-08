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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("position", position_),
				cereal::make_nvp("normal", normal_),
				cereal::make_nvp("tangent", tangent_),
				cereal::make_nvp("texcoord", texcoord_),
				cereal::make_nvp("joints", joints_),
				cereal::make_nvp("weights", weights_)
			);
		}
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("position_xy", positionXY_),
				cereal::make_nvp("position_z_tex_u", positionZTexU_),
				cereal::make_nvp("tex_v_tangent", texVTangent_),
				cereal::make_nvp("normal", normal_)
			);
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("joints_xy", jointsXY_),
				cereal::make_nvp("joints_zw", jointsZW_),
				cereal::make_nvp("weights", weights_)
			);
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("vertex_offset", vertexOffset_),
				cereal::make_nvp("triangle_offset", triangleOffset_),
				cereal::make_nvp("vertex_count", vertexCount_),
				cereal::make_nvp("triangle_count", triangleCount_)
			);
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("center", center_),
				cereal::make_nvp("radius", radius_),
				cereal::make_nvp("cone_axis", coneAxis_),
				cereal::make_nvp("cone_cutoff", coneCutoff_)
			);
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("meshlet_offset", meshletOffset_),
				cereal::make_nvp("meshlet_count", meshletCount_),
				cereal::make_nvp("lod_level", lodLevel_),
				cereal::make_nvp("lod_error", lodError_)
			);
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("cluster_offset", clusterOffset_),
				cereal::make_nvp("cluster_count", clusterCount_),
				cereal::make_nvp("material_index", materialIndex_),
				cereal::make_nvp("index_offset", indexOffset_),
				cereal::make_nvp("index_count", indexCount_),
				cereal::make_nvp("skin_index", skinIndex_)
			);
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("name", name_),
				cereal::make_nvp("nodes", nodes_)
			);
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

			template<class T>
			void serialize(T& archive)
			{
				archive(cereal::make_nvp("emissive_strength", emissiveStrength_));
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

			template<class T>
			void serialize(T& archive)
			{
				archive(cereal::make_nvp("ior", ior_));
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

			template<class T>
			void serialize(T& archive)
			{
				archive(
					cereal::make_nvp("specular_factor", specularFactor_),
					cereal::make_nvp("specular_color_factor", specularColorFactor_),
					cereal::make_nvp("specular_texture_index", specularTextureIndex_),
					cereal::make_nvp("specular_color_texture_index", specularColorTextureIndex_)
				);
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

			template<class T>
			void serialize(T& archive)
			{
				archive(
					cereal::make_nvp("clear_coat_factor", clearCoatFactor_),
					cereal::make_nvp("clear_coat_roughness_factor", clearCoatRoughnessFactor_),
					cereal::make_nvp("clear_coat_texture_index", clearCoatTextureIndex_),
					cereal::make_nvp("clear_coat_roughness_texture_index", clearCoatRoughnessTextureIndex_),
					cereal::make_nvp("clear_coat_normal_texture_index", clearCoatNormalTextureIndex_)
				);
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

			template<class T>
			void serialize(T& archive)
			{
				archive(
					cereal::make_nvp("transmission_factor", transmissionFactor_),
					cereal::make_nvp("transmission_texture_index", transmissionTextureIndex_)
				);
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

			template<class T>
			void serialize(T& archive)
			{
				archive(
					cereal::make_nvp("thickness_factor", thicknessFactor_),
					cereal::make_nvp("attenuation_distance", attenuationDistance_),
					cereal::make_nvp("attenuation_color", attenuationColor_),
					cereal::make_nvp("thickness_texture_index", thicknessTextureIndex_)
				);
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

			template<class T>
			void serialize(T& archive)
			{
				archive(
					cereal::make_nvp("sheen_color_factor", sheenColorFactor_),
					cereal::make_nvp("sheen_roughness_factor", sheenRoughnessFactor_),
					cereal::make_nvp("sheen_color_texture_index", sheenColorTextureIndex_),
					cereal::make_nvp("sheen_roughness_texture_index", sheenRoughnessTextureIndex_)
				);
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

			template<class T>
			void serialize(T& archive)
			{
				archive(
					cereal::make_nvp("iridescence_factor", iridescenceFactor_),
					cereal::make_nvp("iridescence_ior", iridescenceIor_),
					cereal::make_nvp("iridescence_thickness_minimum", iridescenceThicknessMinimum_),
					cereal::make_nvp("iridescence_thickness_maximum", iridescenceThicknessMaximum_),
					cereal::make_nvp("iridescence_texture_index", iridescenceTextureIndex_),
					cereal::make_nvp("iridescence_thickness_texture_index", iridescenceThicknessTextureIndex_)
				);
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

			template<class T>
			void serialize(T& archive)
			{
				archive(
					cereal::make_nvp("anisotropy_strength", anisotropyStrength_),
					cereal::make_nvp("anisotropy_rotation", anisotropyRotation_),
					cereal::make_nvp("anisotropy_texture_index", anisotropyTextureIndex_)
				);
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

			template<class T>
			void serialize(T& archive)
			{
				archive(cereal::make_nvp("unlit", unlit_));
			}
		};
		Unlit unlit_;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("emissive_strength", emissiveStrength_),
				cereal::make_nvp("ior", ior_),
				cereal::make_nvp("specular", specular_),
				cereal::make_nvp("clear_coat", clearCoat_),
				cereal::make_nvp("transmission", transmission_),
				cereal::make_nvp("volume", volume_),
				cereal::make_nvp("sheen", sheen_),
				cereal::make_nvp("iridescence", iridescence_),
				cereal::make_nvp("anisotropy", anisotropy_),
				cereal::make_nvp("unlit", unlit_)
			);
		}
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

		/// [JP] KHR_materials_* 拡張。拡張が無ければ各デフォルト（中立値）のまま。
		KHR khr_;

		Uint32 baseColorTextureIndex_ = 0xFFFFFFFF;
		Uint32 normalTextureIndex_ = 0xFFFFFFFF;
		Uint32 metallicRoughnessTextureIndex_ = 0xFFFFFFFF;
		Uint32 occlusionTextureIndex_ = 0xFFFFFFFF;
		Uint32 emissiveTextureIndex_ = 0xFFFFFFFF;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("base_color", baseColor_),
				cereal::make_nvp("metallic", metallic_),
				cereal::make_nvp("roughness", roughness_),
				cereal::make_nvp("emissive_factor", emissiveFactor_),
				cereal::make_nvp("alpha_mode", alphaMode_),
				cereal::make_nvp("alpha_cutoff", alphaCutoff_),
				cereal::make_nvp("double_sided", doubleSided_),
				cereal::make_nvp("khr", khr_),
				cereal::make_nvp("base_color_texture_index", baseColorTextureIndex_),
				cereal::make_nvp("normal_texture_index", normalTextureIndex_),
				cereal::make_nvp("metallic_roughness_texture_index", metallicRoughnessTextureIndex_),
				cereal::make_nvp("occlusion_texture_index", occlusionTextureIndex_),
				cereal::make_nvp("emissive_texture_index", emissiveTextureIndex_)
			);
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

		Quaternion rotation_ = { 0,0,0,1 };
		Vector3 scale_ = { 1,1,1 };
		Vector3 translation_ = { 0,0,0 };
		Matrix globalTransform_ = Matrix::Identity;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("name", name_),
				cereal::make_nvp("mesh", mesh_),
				cereal::make_nvp("skin", skin_),
				cereal::make_nvp("light", light_),
				cereal::make_nvp("children", children_),
				cereal::make_nvp("rotation", rotation_),
				cereal::make_nvp("scale", scale_),
				cereal::make_nvp("translation", translation_),
				cereal::make_nvp("global_transform", globalTransform_)
			);
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("inverse_bind_matrices", inverseBindMatrices_),
				cereal::make_nvp("joints", joints_)
			);
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
		enum class Type : Uint32
		{
			Point = 0,
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("type", type_),
				cereal::make_nvp("position", position_),
				cereal::make_nvp("direction", direction_),
				cereal::make_nvp("color_rgb", colorRGB_),
				cereal::make_nvp("intensity", intensity_),
				cereal::make_nvp("range", range_),
				cereal::make_nvp("inner_cone_angle", innerConeAngle_),
				cereal::make_nvp("outer_cone_angle", outerConeAngle_)
			);
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("name", name_),
				cereal::make_nvp("width", width_),
				cereal::make_nvp("height", height_),
				cereal::make_nvp("component", component_),
				cereal::make_nvp("bits", bits_),
				cereal::make_nvp("mip_count", mipCount_),
				cereal::make_nvp("mime_type", mimeType_),
				cereal::make_nvp("cache_data", cacheData_)
			);
		}
	};

	/// [EN] Which cluster BakeCollision pulls from a SubMesh: the coarsest
	///      cluster (same range RT already uses for its proxy geometry) or
	///      the LOD 0 cluster.
	/// [JP] BakeCollision が SubMesh のどのクラスタから抽出するか:
	///      最粗クラスタ（RT のプロキシジオメトリと同じ範囲）か LOD 0 クラスタか。
	enum class MeshCollisionDetail
	{
		/// [EN] Each SubMesh's coarsest cluster — same range the RT proxy
		///      geometry already uses.
		/// [JP] 各 SubMesh の最粗クラスタ — RT プロキシジオメトリが既に
		///      使っているのと同じ範囲。
		Proxy,

		/// [EN] Each SubMesh's LOD 0 (most detailed) cluster.
		/// [JP] 各 SubMesh の LOD 0（最も詳細な）クラスタ。
		Full,
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

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("compressed_vertices", compressedVertices_),
				cereal::make_nvp("compressed_skin_vertices", compressedSkinVertices_),
				cereal::make_nvp("position_min", positionMin_),
				cereal::make_nvp("position_extent", positionExtent_),
				cereal::make_nvp("texcoord_min", texcoordMin_),
				cereal::make_nvp("texcoord_extent", texcoordExtent_),
				cereal::make_nvp("vertex_indices", vertexIndices_),
				cereal::make_nvp("primitive_indices", primitiveIndices_),
				cereal::make_nvp("meshlets", meshlets_),
				cereal::make_nvp("meshlet_bounds", meshletBounds_),
				cereal::make_nvp("clusters", clusters_),
				cereal::make_nvp("stages", stages_),
				cereal::make_nvp("sub_meshes", subMeshes_),
				cereal::make_nvp("materials", materials_),
				cereal::make_nvp("nodes", nodes_),
				cereal::make_nvp("skins", skins_),
				cereal::make_nvp("bitmaps", bitmaps_),
				cereal::make_nvp("lights", lights_),
				cereal::make_nvp("default_stage", defaultStage_)
			);
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
		void BakeBitmap(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BC7CompressShader& bc7Shader);

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
		[[nodiscard]] Bool SoftbodyVertices(DynamicArray<Vertex>& outVertices, DynamicArray<Uint32>& outIndices)const;

		/**
		* [EN]
		* Reads each SubMesh's coarsest cluster (same range BakeCollision's
		* Proxy detail and the RT proxy geometry use) into a CPU-side
		* full-vertex/index pair, compacted/deduped into its own local
		* index space (unlike SoftbodyVertices, which keeps the full LOD 0
		* global index space uncompacted). For a Softbody to stay real-time
		* on a render-resolution mesh (thousands of vertices, one PBD edge/
		* shear/bend constraint per edge — far past what Jolt's soft body
		* solver is meant for), simulation runs on this much smaller proxy
		* mesh instead; SoftbodyMesh binds each render vertex (from
		* SoftbodyVertices) to its nearest proxy vertices at build time and
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
		* SoftbodyVertices とは異なる）。Softbody がレンダー解像度の
		* メッシュ（数千頂点、辺ごとに PBD の辺/シア/曲げ拘束1つ —
		* Jolt のソフトボディソルバーが想定する規模をはるかに超える）で
		* リアルタイムを保つため、シミュレーションはこのずっと小さい
		* プロキシメッシュ上で行う。SoftbodyMesh がビルド時に各レンダー
		* 頂点（SoftbodyVertices 由来）を最も近いプロキシ頂点群へ束縛し、
		* 毎フレームそのシミュレート済み変位をブレンドしてフル解像度
		* メッシュへ適用するので、メッシュシェーダは引き続きフル
		* ディテールで描画する。ベイク/キャッシュパイプラインの一部では
		* なく、ロード済みの Crister に対してその都度呼び出す。抽出対象が
		* 無ければ false を返す。
		*/
		[[nodiscard]] Bool SoftbodyProxyVertices(DynamicArray<Vertex>& outVertices, DynamicArray<Uint32>& outIndices)const;

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
		void Upload(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap);

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
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS VertexBufferGPUAddress()const;

		/**
		* [EN]
		* Vertex count of the RT proxy position buffer VertexBufferGPUAddress() points to.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* VertexBufferGPUAddress() が指す RT プロキシ位置バッファの頂点数。
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
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS FlatTriangleIndexBufferGPUAddress()const;

		/**
		* [EN]
		* Triangle count of the flat index buffer FlatTriangleIndexBufferGPUAddress() points to.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* FlatTriangleIndexBufferGPUAddress() が指すフラットインデックスバッファの三角形数。
		*/
		[[nodiscard]] Uint32 FlatTriangleIndexCount()const;


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
		[[nodiscard]] Uint FlatTriangleIndexShaderResourceViewIndex()const;

		/**
		* [EN]
		* Whether this Crister has RT-side skinning data (skinVertexResource_/
		* rtSkinVertexResource_ populated), i.e. skins_ is non-empty and the
		* RT proxy build found at least one skinned SubMesh.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この Crister が RT 側のスキニングデータを持つか
		* (skinVertexResource_/rtSkinVertexResource_ が構築済みか)。
		* skins_ が空でなく、RT プロキシ構築時にスキンド SubMesh を
		* 1つ以上見つけた場合に true。
		*/
		[[nodiscard]] Bool HasSkinnedRTGeometry()const;

		/**
		* [EN]
		* GPU address of the RT proxy's decoded float3 position buffer
		* (positionResource_) — see VertexBufferGPUAddress's comment for why
		* RT uses a separate dedicated position buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* RT プロキシのデコード済み float3 位置バッファ (positionResource_)
		* の GPU アドレス — RT が専用の位置バッファを使う理由は
		* VertexBufferGPUAddress のコメント参照。
		*/
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS RTPositionBufferGPUAddress()const;

		/**
		* [EN]
		* GPU address of the RT proxy's skin vertex pool
		* (rtSkinVertexResource_), or 0 when HasSkinnedRTGeometry is false.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* RT プロキシのスキン頂点プール (rtSkinVertexResource_) の
		* GPU アドレス。HasSkinnedRTGeometry が false なら 0。
		*/
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS RTSkinVertexBufferGPUAddress()const;

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
		* Total mip levels the texture has (baked mip chain length), or 0
		* if textureIndex is out of range.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* テクスチャが持つ総ミップ段数（焼き込み済みミップチェーンの長さ）。
		* textureIndex が範囲外なら 0。
		*/
		[[nodiscard]] Uint32 TextureMipCount(Uint32 textureIndex)const;

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
		[[nodiscard]] Uint32 TextureTopResidentMip(Uint32 textureIndex)const;

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
		[[nodiscard]] Uint32 DesiredTextureMip(Uint32 textureIndex, Float worldScale, Float pixelsPerUnit)const;

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
		* VRAM usage by up to mipCount_ per texture once DesiredTextureMip
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
		* 常駐させ続けると、DesiredTextureMip が正しくミップ0へ収束する
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
		Microsoft::WRL::ComPtr<ID3D12Resource> flatTriangleIndexResource_;

		Microsoft::WRL::ComPtr<ID3D12Resource> rtSkinVertexResource_;

		Uint vertexBufferIndex_ = 0xFFFFFFFF;
		Uint skinVertexBufferIndex_ = 0xFFFFFFFF;
		Uint flatTriangleIndexShaderResourceViewIndex_ = 0;
		Uint32 flatTriangleIndexCount_ = 0;
		Uint32 rtVertexCount_ = 0;

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

		/// [EN] Captured at Upload() so pages can stream in and out later.
		///      These engine objects outlive every Crister.
		/// [JP] 後からページを出し入れできるよう Upload() で保持する。
		///      これらのエンジンオブジェクトは全 Crister より長寿命。
		ID3D12Device* device_ = nullptr;
		ID3D12CommandQueue* uploadQueue_ = nullptr;
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