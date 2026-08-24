#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <FoundationEngine/Resource/AxisConvention.h>
#include <GraphicsEngine/Model/Crister.h>

namespace SeedCore
{
	struct LoaderSystem;
	class BindlessHeap;
	class BC7CompressShader;
	class D3D12CommandQueue;

	/**
	* [EN]
	* Loads glTF/GLB 3D models and converts them into the engine's internal
	* Crister format (meshlets + LOD hierarchy + serialisable cache).
	*
	* Responsibilities:
	*   - Parse glTF/GLB via tinygltf (or load from ".crister" binary cache)
	*   - Extract scenes, nodes, meshes, materials, skins, textures
	*   - Generate meshlets (64 vertices / 124 triangles per workgroup)
	*   - Build a multi-level LOD hierarchy using QEM simplification
	*   - Compute per-meshlet bounding spheres and normal cones
	*   - Serialise the result as a ".crister" cache for fast future loads
	*   - Trigger GPU upload via Crister::Upload
	*
	* Crister objects are pooled in a StablePool and accessed via Handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF/GLB 3D モデルを読み込み、エンジン内部の Crister フォーマット
	* （メシュレット + LOD 階層 + シリアライズ可能キャッシュ）に変換する。
	*
	* 担当:
	*   - tinygltf で glTF/GLB を解析（または ".crister" バイナリキャッシュから読み込み）
	*   - シーン、ノード、メッシュ、マテリアル、スキン、テクスチャの抽出
	*   - メシュレット生成（ワークグループあたり 64 頂点 / 124 三角形）
	*   - QEM 簡略化によるマルチレベル LOD 階層の構築
	*   - メシュレットごとの包囲球と法線コーンの計算
	*   - 次回の高速ロードのため ".crister" キャッシュとしてシリアライズ
	*   - Crister::Upload による GPU アップロードの起動
	*
	* Crister オブジェクトは StablePool でプールされ、Handle でアクセスする。
	*/
	class ModelLoader :public NonCopyable
	{
	public:
		ModelLoader() = default;
		~ModelLoader() = default;

		/**
		* [EN]
		* Loads a 3D model from a glTF/GLB file path.
		* Returns a handle to the loaded Crister, or a null handle on failure.
		* If a ".crister" cache exists next to the source file, loads from
		* cache instead of re-parsing the glTF.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* glTF/GLB ファイルパスから 3D モデルを読み込む。
		* 読み込まれた Crister へのハンドルを返す。失敗時は null ハンドル。
		* ソースファイルの隣に ".crister" キャッシュがあれば、glTF を再解析
		* せずキャッシュから読み込む。axisConvention は glTF を再解析する
		* 場合のみ適用される（キャッシュ読み込み時は無視される）。
		*/
		Handle<Crister> Load(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, String filePath, const AxisConvention& axisConvention = AxisConvention{});

		/**
		* [EN]
		* Retrieves a Crister pointer from a handle. Returns nullptr if invalid.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ハンドルから Crister ポインタを取得する。無効なら nullptr を返す。
		*/
		Crister* Get(const Handle<Crister>& handle);

		/**
		* [EN]
		* Releases a Crister and returns the handle to the pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Crister を解放し、ハンドルをプールに返却する。
		*/
		void Clear(Handle<Crister>& handle, BindlessHeap* heap)noexcept;

	private:
		/**
		* [EN]
		* Extracts scene definitions (root node lists + default scene) from glTF.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* glTF からシーン定義（ルートノードリスト + デフォルトシーン）を抽出する。
		*/
		void FetchStages(const tinygltf::Model& model, Crister& crister);

		/**
		* [EN]
		* Extracts node hierarchy and transforms (S/R/T or matrix) from glTF.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* glTF からノード階層とトランスフォーム（S/R/T または行列）を抽出する。
		*/
		void FetchNodes(const tinygltf::Model& model, Crister& crister);

		/**
		* [EN]
		* Extracts vertex attributes and index buffers for all mesh primitives.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全メッシュプリミティブの頂点属性とインデックスバッファを抽出する。
		*/
		void FetchMeshes(const tinygltf::Model& model, Crister& crister);

		/**
		* [EN]
		* Extracts PBR materials and resolves texture → image indices.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* PBR マテリアルを抽出し、テクスチャ → イメージインデックスを解決する。
		*/
		void FetchMaterials(const tinygltf::Model& model, Crister& crister);

		/**
		* [EN]
		* Extracts skeleton data (joint lists + inverse bind matrices).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スケルトンデータ（ジョイントリスト + 逆バインド行列）を抽出する。
		*/
		void FetchSkins(const tinygltf::Model& model, Crister& crister);

		/**
		* [EN]
		* Resolves which skin (if any) each SubMesh belongs to, by walking the
		* glTF nodes that reference a mesh + skin pair.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* mesh + skin のペアを参照する glTF ノードを走査し、各 SubMesh が
		* どのスキンに属するか（属さないか）を解決する。
		*/
		void ResolveSubMeshSkins(const tinygltf::Model& model, Crister& crister);

		/**
		* [EN]
		* Copies decoded image data from glTF into Crister's texture array.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* glTF からデコード済み画像データを Crister のテクスチャ配列にコピーする。
		*/
		void FetchTexture(const tinygltf::Model& model, Crister& crister);

		/**
		* [EN]
		* Generates meshlets and multi-level LOD hierarchy (QEM + DAG clusters).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* メシュレットとマルチレベル LOD 階層（QEM + DAG クラスター）を生成する。
		*/
		void BuildMeshlets(Crister& crister);

		/**
		* [EN]
		* Walks the node tree depth-first to compute world-space transforms.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ノードツリーを深さ優先で走査し、ワールド空間トランスフォームを計算する。
		*/
		void CumulateTransforms(Crister& crister);

		/**
		* [EN]
		* Converts every node/vertex/skin in crister from its source axis
		* convention into the engine's fixed LH/Y-up/Z-forward space, using
		* the basis resolved from axisConvention. Falls back to the default
		* AxisConvention if axisConvention resolves to an invalid basis
		* (never silently skips conversion).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* crister 内の全ノード/頂点/スキンを、axisConvention から解決された
		* 基底を用いて、ソースの軸コンベンションからエンジン固定の
		* LH/Y-up/Z-forward 空間へ変換する。axisConvention が無効な基底に
		* 解決された場合はデフォルトの AxisConvention にフォールバックする
		* （変換を黙ってスキップすることはない）。
		*/
		void ConvertAxisConvention(Crister& crister, const AxisConvention& axisConvention);

		/**
		* [EN]
		* Extracts KHR_lights_punctual point/spot lights, resolving each
		* referencing node's world position/direction. Must run after
		* ConvertAxisConvention (its own CumulateTransforms call) so
		* Node::globalTransform_ is already in engine (LH) space.
		* Directional lights are skipped: this engine's single directional
		* light is authored per-scene, not per-model.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KHR_lights_punctual のポイント/スポットライトを抽出し、参照ノードの
		* ワールド位置/向きを解決する。ConvertAxisConvention（内部で
		* CumulateTransforms を再実行する）の後に呼ぶこと。Node::globalTransform_
		* が既にエンジン（左手系）空間になっている必要があるため。
		* ディレクショナルライトはスキップする: このエンジンの単一
		* ディレクショナルライトはモデル単位ではなくシーン単位で設定する。
		*/
		void FetchLights(const tinygltf::Model& model, Crister& crister);

		/// [EN] Transforms a position/direction Vector3 by a change-of-basis matrix (v' = Vector3::Transform(v, basis)).
		/// [JP] 位置/方向を表す Vector3 を基底変換行列で変換する (v' = Vector3::Transform(v, basis))。
		void ConvertPositionByBasis(Vector3& vector, const Matrix& basis);

		/// [EN] Transforms the .xyz of a Vector4 (e.g. tangent) by basis; .w (handedness sign) is not touched here.
		/// [JP] Vector4（タンジェント等）の .xyz を basis で変換する。.w（利き手符号）はここでは触らない。
		void ConvertPositionByBasis(Vector4& vector, const Matrix& basis);

		/// [EN] Conjugates a rotation quaternion by an orthogonal change-of-basis: R' = basis^T * R * basis.
		/// [JP] 回転クォータニオンを直交基底変換で共役変換する: R' = basis^T * R * basis。
		void ConvertRotationByBasis(Quaternion& quaternion, const Matrix& basis);

		/// [EN] Conjugates a full 4x4 transform (e.g. an inverse bind matrix) by basis: matrix' = basis^T * matrix * basis.
		/// [JP] 4x4変換（逆バインド行列等）を basis で共役変換する: matrix' = basis^T * matrix * basis。
		void ConvertMatrixByBasis(Matrix& matrix, const Matrix& basis);

		/// [EN] Swaps two corners of every triangle in crister.vertexIndices_ (pre-meshlet, flat global index list), reversing winding order. Only called when the resolved conversion explicitly requires it.
		/// [JP] crister.vertexIndices_（メシュレット化前のフラットなグローバルインデックス列）の全三角形について2頂点を入れ替え、巻き順を反転する。解決済みの変換が明示的に要求する時のみ呼ばれる。
		void FlipTriangleWinding(Crister& crister);

	private:
		/// [EN] Object pool for Crister instances. Provides stable pointers and handle-based access.
		/// [JP] Crister インスタンスのオブジェクトプール。安定したポインタとハンドルベースのアクセスを提供する。
		StablePool<Crister> pool_;
	};
}
