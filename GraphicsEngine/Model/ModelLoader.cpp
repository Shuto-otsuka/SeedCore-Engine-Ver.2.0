#include <GraphicsEngine/Model/ModelLoader.h>
#include <GraphicsEngine/Model/QuadricErrorMetrics.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	/**
	* [EN]
	* Loads a 3D model from a glTF/GLB file and returns a handle to the
	* resulting Crister object.
	*
	* Flow:
	*   1. If a pre-built ".crister" binary cache exists alongside the
	*      source file, deserialise from cache (fast path).
	*   2. Otherwise, parse the glTF/GLB with tinygltf, extract all data
	*      (scenes, nodes, meshes, materials, skins, textures), generate
	*      meshlets + LOD hierarchy, then serialise the result as a
	*      ".crister" cache for future loads.
	*   3. Upload GPU resources via Crister::Upload.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF/GLB ファイルから 3D モデルを読み込み、結果の Crister オブジェクトへの
	* ハンドルを返す。
	*
	* フロー:
	*   1. ソースファイルと同じ場所に ".crister" バイナリキャッシュがあれば
	*      キャッシュからデシリアライズする（高速パス）。
	*   2. なければ tinygltf で glTF/GLB を解析し、全データ
	*      （シーン、ノード、メッシュ、マテリアル、スキン、テクスチャ）を抽出し、
	*      メシュレット + LOD 階層を生成した後、".crister" キャッシュとして
	*      シリアライズして次回のロードに備える。
	*   3. Crister::Upload で GPU リソースをアップロードする。
	*/
	Handle<Crister> ModelLoader::Load(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, String filePath, const AxisConvention& axisConvention)
	{
		Handle<Crister> handle = pool_.Create();
		Crister* crister = pool_.Get(handle);
		if (!crister)
		{
			return Handle<Crister>::null();
		}

		std::filesystem::path path(filePath.c_str());
		std::filesystem::path cristerPath = path;
		cristerPath.replace_extension(".crister");

		/// [EN] Source-preferred: only read the binary cache when the assigned
		///      asset IS the ".crister" itself. Assigning the source glTF/GLB
		///      always re-parses it (source of truth) and re-bakes the sibling
		///      ".crister". A cache written by an older serialisation layout
		///      throws cereal::Exception — treat it as a cache miss.
		/// [JP] ソース優先: 割り当てられたアセットが ".crister" 自身の時だけ
		///      バイナリキャッシュを読む。ソース glTF/GLB を割り当てた時は常に
		///      再解析し（source of truth）隣の ".crister" を再ベイクする。古い
		///      シリアライズレイアウトのキャッシュは cereal::Exception を投げる
		///      ため、キャッシュミス扱いにする。
		Bool loadedFromCache = false;
		if (path.extension() == ".crister" && std::filesystem::exists(cristerPath))
		{
			try
			{
				std::ifstream ifs(cristerPath, std::ios::binary);
				cereal::BinaryInputArchive archive(ifs);
				archive(*crister);
				loadedFromCache = true;
			}
			catch (const cereal::Exception&)
			{
				*crister = Crister{};
			}
		}

		if (!loadedFromCache)
		{
			/// [EN] Slow path: parse the original glTF/GLB file with tinygltf.
			/// [JP] 低速パス: tinygltf で元の glTF/GLB ファイルを解析する。
			tinygltf::Model model;
			tinygltf::TinyGLTF tinyLoader;
			std::string error, warning;

			Bool result = false;
			std::string pathString = path.string();
			if (path.extension() == ".glb")
			{
				result = tinyLoader.LoadBinaryFromFile(&model, &error, &warning, pathString);
			}
			else
			{
				result = tinyLoader.LoadASCIIFromFile(&model, &error, &warning, pathString);
			}

			if (!result)
			{
				pool_.Destroy(handle);
				return Handle<Crister>::null();
			}

			/// [EN] Extract all data from the tinygltf model into Crister's flat arrays.
			/// [JP] tinygltf モデルから全データを Crister のフラット配列に抽出する。
			FetchStages(model, *crister);
			FetchNodes(model, *crister);
			FetchMaterials(model, *crister);
			FetchMeshes(model, *crister);
			FetchSkins(model, *crister);
			ResolveSubMeshSkins(model, *crister);
			FetchTexture(model, *crister);

			ConvertAxisConvention(*crister, axisConvention);

			/// [EN] Resolves KHR_lights_punctual nodes to world-space light data.
			///      Must run after ConvertAxisConvention so Node::globalTransform_
			///      is already in engine (LH) space.
			/// [JP] KHR_lights_punctual ノードをワールド空間のライトデータに解決する。
			///      Node::globalTransform_ が既にエンジン（左手系）空間になっている
			///      必要があるため ConvertAxisConvention の後に実行する。
			FetchLights(model, *crister);

			/// [EN] Generate meshlets and LOD hierarchy from the extracted mesh data.
			/// [JP] 抽出したメッシュデータからメシュレットと LOD 階層を生成する。
			BuildMeshlets(*crister);

			/// [EN] Quantise vertices_ (now including LOD-duplicated copies) into
			///      the GPU format and bake it into the cache, freeing vertices_.
			/// [JP] vertices_（LOD 複製後の最終集合）を GPU フォーマットへ量子化し
			///      キャッシュへ焼き込む。vertices_ はここで解放される。
			crister->BakeMesh();

			/// [EN] Downsample/dilate/BC7-compress every texture and bake the
			///      result into the cache. Independent of BakeMesh; order
			///      between the two doesn't matter.
			/// [JP] 全テクスチャをダウンサンプル/dilation/BC7 圧縮してキャッシュへ
			///      焼き込む。BakeMesh とは独立しており、両者の順序は問わない。
			crister->BakeBitmap(device, cmdQueue, bc7Shader);

			/// [EN] Serialise everything to ".crister" binary cache for next load.
			/// [JP] 次回のロードのため ".crister" バイナリキャッシュにシリアライズする。
			std::ofstream ofs(cristerPath, std::ios::binary);
			cereal::BinaryOutputArchive archive(ofs);
			archive(*crister);
		}

		/// [EN] Upload vertex/index/meshlet buffers to GPU.
		/// [JP] 頂点・インデックス・メシュレットバッファを GPU にアップロードする。
		crister->Upload(device, cmdQueue, heap);

		return handle;
	}

	Crister* ModelLoader::Get(const Handle<Crister>& handle)
	{
		return pool_.Get(handle);
	}

	void ModelLoader::Clear(Handle<Crister>& handle, BindlessHeap* heap)noexcept
	{
		pool_.Destroy(handle);
	}

	/**
	* [EN]
	* Extracts scene data from the glTF model. Each scene contains a list
	* of root node indices. Also records which scene is the default.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF モデルからシーンデータを抽出する。各シーンにはルートノードの
	* インデックスリストが含まれる。デフォルトシーンも記録する。
	*/
	void ModelLoader::FetchStages(const tinygltf::Model& model, Crister& crister)
	{
		for (const tinygltf::Scene& gltfScene : model.scenes)
		{
			Stage& stage = crister.stages_.emplace_back();
			stage.name_ = gltfScene.name;
			stage.nodes_ = gltfScene.nodes;
		}
		crister.defaultStage_ = model.defaultScene < 0 ? 0 : model.defaultScene;
	}

	/**
	* [EN]
	* Extracts node hierarchy from the glTF model.
	*
	* Each glTF node can store its transform in one of two ways:
	*   - A 4×4 matrix (decomposed into S/R/T here)
	*   - Separate scale, rotation (quaternion), and translation arrays
	*
	* After extraction, CumulateTransforms is called to compute the
	* global (world-space) transform for each node by walking the tree.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF モデルからノード階層を抽出する。
	*
	* 各 glTF ノードはトランスフォームを 2 通りのどちらかで格納する:
	*   - 4×4 行列（ここで S/R/T に分解）
	*   - スケール、回転（クォータニオン）、平行移動の個別配列
	*
	* 抽出後、CumulateTransforms を呼んでツリーを走査し、各ノードの
	* グローバル（ワールド空間）トランスフォームを計算する。
	*/
	void ModelLoader::FetchNodes(const tinygltf::Model& model, Crister& crister)
	{
		for (const tinygltf::Node& gltfNode : model.nodes)
		{
			Node& node = crister.nodes_.emplace_back();
			node.name_ = gltfNode.name;
			node.skin_ = gltfNode.skin;
			node.mesh_ = gltfNode.mesh;
			node.children_ = gltfNode.children;

			/// [EN] KHR_lights_punctual: the per-node light reference isn't a
			///      first-class tinygltf::Node member, so read it from the
			///      node's own extensions map. Resolved into world-space
			///      light data later, by FetchLights (after ConvertAxisConvention).
			/// [JP] KHR_lights_punctual: ノードごとのライト参照は tinygltf::Node
			///      の正式なメンバではないため、ノード自身の extensions マップ
			///      から読む。ワールド空間のライトデータへの解決は後段の
			///      FetchLights（ConvertAxisConvention の後）で行う。
			if (auto it = gltfNode.extensions.find("KHR_lights_punctual"); it != gltfNode.extensions.end())
			{
				if (it->second.Has("light"))
				{
					node.light_ = static_cast<Int>(it->second.Get("light").GetNumberAsDouble());
				}
			}

			if (!gltfNode.matrix.empty())
			{
				/// [EN] The node stores a full 4×4 matrix. Decompose into scale/rotation/translation.
				/// [JP] ノードが 4×4 行列を格納している。スケール/回転/平行移動に分解する。
				/// [EN] Sequential copy is correct (same reasoning as the inverse bind
				///      matrices in FetchSkins): glTF column-major data maps directly onto the
				///      engine's row-major row-vector Matrix. Verified against a reference
				///      implementation. Do NOT swap the indices.
				/// [JP] 連番コピーで正しい（FetchSkins の逆バインド行列と同じ理屈）:
				///      glTF の column-major データはエンジンの row-major 行ベクトル Matrix に
				///      そのまま対応する。参照実装と照合済み。インデックスを入れ替えないこと。
				Matrix matrix{};
				for (Size row = 0;row < 4;row++)
				{
					for (Size column = 0;column < 4;column++)
					{
						matrix.m[row][column] = static_cast<Float>(gltfNode.matrix.at(4 * row + column));
					}
				}
				Vector3 cacheScale, cacheTranslation;
				Quaternion cacheRotation;
				matrix.Decompose(cacheScale, cacheRotation, cacheTranslation);
				node.scale_ = cacheScale;
				node.rotation_ = cacheRotation;
				node.translation_ = cacheTranslation;
			}
			else
			{
				/// [EN] The node stores separate S/R/T components. Copy them directly.
				/// [JP] ノードが S/R/T を個別に格納している。そのままコピーする。
				if (gltfNode.scale.size() == 3)
				{
					node.scale_.x = static_cast<Float>(gltfNode.scale[0]);
					node.scale_.y = static_cast<Float>(gltfNode.scale[1]);
					node.scale_.z = static_cast<Float>(gltfNode.scale[2]);
				}
				if (gltfNode.rotation.size() == 4)
				{
					node.rotation_.x = static_cast<Float>(gltfNode.rotation[0]);
					node.rotation_.y = static_cast<Float>(gltfNode.rotation[1]);
					node.rotation_.z = static_cast<Float>(gltfNode.rotation[2]);
					node.rotation_.w = static_cast<Float>(gltfNode.rotation[3]);
				}
				if (gltfNode.translation.size() == 3)
				{
					node.translation_.x = static_cast<Float>(gltfNode.translation[0]);
					node.translation_.y = static_cast<Float>(gltfNode.translation[1]);
					node.translation_.z = static_cast<Float>(gltfNode.translation[2]);
				}
			}
		}

		for (Size nodeIndex = 0; nodeIndex < crister.nodes_.size(); nodeIndex++)
		{
			for (Int childIndex : crister.nodes_[nodeIndex].children_)
			{
				if (childIndex >= 0 && static_cast<Size>(childIndex) < crister.nodes_.size())
				{
					crister.nodes_[childIndex].parentIndex_ = static_cast<Int>(nodeIndex);
				}
			}
		}

		CumulateTransforms(crister);
	}

	/**
	* [EN]
	* Extracts all mesh primitives from the glTF model.
	*
	* Each glTF mesh contains one or more "primitives" (sub-meshes), each
	* with its own material and vertex attributes. This function:
	*
	*   1. Reads vertex attributes (POSITION, NORMAL, TANGENT, TEXCOORD_0,
	*      JOINTS_0, WEIGHTS_0) from the glTF binary buffers.
	*   2. Reads the index buffer. If the primitive has no index buffer
	*      (non-indexed), generates sequential indices (0, 1, 2, ...).
	*   3. Records indexOffset_ and indexCount_ on each SubMesh so
	*      BuildMeshlets knows which slice of vertexIndices_ belongs to
	*      which SubMesh.
	*
	* glTF stores joint indices and weights with varying component types
	* (UNSIGNED_INT, UNSIGNED_SHORT, UNSIGNED_BYTE, FLOAT), so they are
	* read with type-specific code instead of the generic fetchAttribute
	* lambda.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF モデルから全メッシュプリミティブを抽出する。
	*
	* 各 glTF メッシュには 1 つ以上の「プリミティブ」（サブメッシュ）が
	* 含まれ、それぞれが独自のマテリアルと頂点属性を持つ。この関数は:
	*
	*   1. glTF バイナリバッファから頂点属性（POSITION, NORMAL, TANGENT,
	*      TEXCOORD_0, JOINTS_0, WEIGHTS_0）を読み取る。
	*   2. インデックスバッファを読み取る。プリミティブにインデックスバッファが
	*      ない場合（非インデックス）、連番インデックス (0, 1, 2, ...) を生成する。
	*   3. 各 SubMesh に indexOffset_ と indexCount_ を記録し、
	*      BuildMeshlets が vertexIndices_ のどの範囲が対応するかを知れるようにする。
	*
	* glTF はジョイントインデックスとウェイトを様々なコンポーネント型
	* （UNSIGNED_INT, UNSIGNED_SHORT, UNSIGNED_BYTE, FLOAT）で格納するため、
	* 汎用の fetchAttribute ラムダではなく型別コードで読み取る。
	*/
	void ModelLoader::FetchMeshes(const tinygltf::Model& model, Crister& crister)
	{
		/// [EN] Lazily-created shared default material, used when a primitive has
		///      no material (gltfPrimitive.material == -1). Per the glTF spec this
		///      is a valid state that must fall back to a default material, not be
		///      reinterpreted as an index: assigning -1 directly into the Uint32
		///      materialIndex_ would wrap to 0xFFFFFFFF and cause an out-of-bounds
		///      read in ModelRenderer/TimelineRenderer/ModelTransformRenderer's
		///      materials[subMesh.materialIndex_].
		/// [JP] プリミティブがマテリアルを持たない場合（gltfPrimitive.material == -1）
		///      に使う、遅延生成の共有デフォルトマテリアル。glTF 仕様上これは正当な
		///      状態でありデフォルトマテリアルへフォールバックすべきで、そのまま
		///      インデックスとして解釈してはならない: -1 を Uint32 の materialIndex_
		///      にそのまま代入すると 0xFFFFFFFF にラップし、
		///      ModelRenderer/TimelineRenderer/ModelTransformRenderer の
		///      materials[subMesh.materialIndex_] で範囲外アクセスを引き起こす。
		Int defaultMaterialIndex = -1;

		for (Size meshIndex = 0; meshIndex < model.meshes.size(); meshIndex++)
		{
			const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];
			for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
			{
				SubMesh& subMesh = crister.subMeshes_.emplace_back();
				subMesh.meshIndex_ = static_cast<Int>(meshIndex);
				if (gltfPrimitive.material >= 0)
				{
					subMesh.materialIndex_ = static_cast<Uint32>(gltfPrimitive.material);
				}
				else
				{
					if (defaultMaterialIndex < 0)
					{
						defaultMaterialIndex = static_cast<Int>(crister.materials_.size());
						crister.materials_.emplace_back();
					}
					subMesh.materialIndex_ = static_cast<Uint32>(defaultMaterialIndex);
				}

				Uint32 vertexOffset = static_cast<Uint32>(crister.vertices_.size());
				if (gltfPrimitive.attributes.find("POSITION") == gltfPrimitive.attributes.end())
				{
					continue;
				}

				const tinygltf::Accessor& positionAccessor = model.accessors.at(gltfPrimitive.attributes.at("POSITION"));
				Size vertexCount = positionAccessor.count;
				Size baseVertex = crister.vertices_.size();
				crister.vertices_.resize(baseVertex + vertexCount);

				/// [EN] Identity-initialise this range of vertexMorphSource_:
				///      each of these ORIGINAL vertices is its own source
				///      (see vertexMorphSource_'s comment). BuildMeshlets
				///      propagates this forward for any LOD-duplicated copy
				///      it appends later.
				/// [JP] vertexMorphSource_ のこの範囲を恒等初期化する:
				///      これらのオリジナル頂点はそれぞれ自分自身が
				///      ソースになる(vertexMorphSource_ のコメント参照)。
				///      BuildMeshlets が後で追加する LOD 複製コピーへは
				///      これを伝播する。
				crister.vertexMorphSource_.resize(baseVertex + vertexCount);
				for (Size index = 0; index < vertexCount; index++)
				{
					crister.vertexMorphSource_[baseVertex + index] = static_cast<Uint32>(baseVertex + index);
				}

				subMesh.vertexOffset_ = vertexOffset;
				subMesh.vertexCount_ = static_cast<Uint32>(vertexCount);

				/**
				* [EN]
				* Decodes a single scalar component of a glTF accessor into a
				* Float, honouring componentType and the "normalized" flag.
				* POSITION/NORMAL/TANGENT/TEXCOORD_0 are FLOAT in a plain glTF
				* export, but KHR_mesh_quantization (emitted by size-optimising
				* exporters such as gltfpack — used by some marketplace
				* pipelines, e.g. Epic's Fab) stores POSITION as normalized
				* BYTE/SHORT and NORMAL/TANGENT/TEXCOORD_0 as normalized
				* BYTE/UNSIGNED_BYTE/SHORT/UNSIGNED_SHORT to shrink the file.
				*
				* ---------------------------------------------------------------------
				*
				* [JP]
				* glTF アクセサの1コンポーネントを、componentType と
				* "normalized" フラグに従って Float にデコードする。
				* POSITION/NORMAL/TANGENT/TEXCOORD_0 は素の glTF では FLOAT だが、
				* KHR_mesh_quantization（gltfpack のようなサイズ最適化エクスポータ
				* — 一部マーケットプレイスのパイプライン、例えば Epic の Fab が使用
				* — が出力する）はファイルサイズを縮小するため POSITION を正規化
				* BYTE/SHORT、NORMAL/TANGENT/TEXCOORD_0 を正規化
				* BYTE/UNSIGNED_BYTE/SHORT/UNSIGNED_SHORT で格納する。
				*/
				auto decodeComponent = [](const Uchar* bytes, Int componentType, Bool normalized) -> Float
					{
						switch (componentType)
						{
						case TINYGLTF_COMPONENT_TYPE_FLOAT:
							return *reinterpret_cast<const Float*>(bytes);
						case TINYGLTF_COMPONENT_TYPE_BYTE:
						{
							Int8 raw = *reinterpret_cast<const Int8*>(bytes);
							return normalized ? Max(static_cast<Float>(raw) / 127.0f, -1.0f) : static_cast<Float>(raw);
						}
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						{
							Uchar raw = *bytes;
							return normalized ? static_cast<Float>(raw) / 255.0f : static_cast<Float>(raw);
						}
						case TINYGLTF_COMPONENT_TYPE_SHORT:
						{
							Int16 raw = *reinterpret_cast<const Int16*>(bytes);
							return normalized ? Max(static_cast<Float>(raw) / 32767.0f, -1.0f) : static_cast<Float>(raw);
						}
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						{
							Ushort raw = *reinterpret_cast<const Ushort*>(bytes);
							return normalized ? static_cast<Float>(raw) / 65535.0f : static_cast<Float>(raw);
						}
						default:
							return 0.0f;
						}
					};

				/**
				* [EN]
				* Generic attribute fetcher. Decodes every component of a glTF
				* accessor into the corresponding Float-based member of each
				* Vertex struct, for any componentType the glTF spec allows on
				* these attributes (see decodeComponent above) — not just
				* FLOAT. The previous implementation always memcpy'd
				* sizeof(VectorN) raw bytes assuming a FLOAT accessor; against
				* a quantized (non-FLOAT) accessor this silently reinterpreted
				* unrelated integer bit patterns as floats instead of failing,
				* corrupting the mesh (e.g. into a flattened, plank-like shape)
				* without ever throwing.
				*
				* - source:         pointer into the raw glTF buffer
				* - sourceStride:   byte stride between consecutive elements in the glTF buffer
				* - componentCount: number of scalar components (2/3/4), matching the member's Vector width
				*
				* ---------------------------------------------------------------------
				*
				* [JP]
				* 汎用属性フェッチャー。glTF アクセサの全コンポーネントを、これらの
				* 属性で glTF 仕様が許容する任意の componentType について
				* （上記 decodeComponent 参照）各 Vertex 構造体の対応する Float
				* ベースのメンバーへデコードする — FLOAT に限らない。以前の実装は
				* FLOAT アクセサを前提に sizeof(VectorN) の生バイトを常に memcpy
				* しており、量子化された（非 FLOAT の）アクセサに対しては失敗する
				* ことなく無関係な整数ビットパターンを float として黙って
				* 再解釈し、メッシュを（例えば板状に潰れた形へ）破壊していた。
				*
				* - source:         glTF 生バッファへのポインタ
				* - sourceStride:   glTF バッファ内の連続する要素間のバイトストライド
				* - componentCount: スカラーコンポーネント数（2/3/4）、メンバーの Vector 幅と一致
				*/
				auto fetchAttribute = [&](const std::string& name, auto memberPtr, Size componentCount)
					{
						auto it = gltfPrimitive.attributes.find(name);
						if (it == gltfPrimitive.attributes.end())
						{
							return;
						}

						const tinygltf::Accessor& accessor = model.accessors.at(it->second);
						const tinygltf::BufferView& bufferView = model.bufferViews.at(accessor.bufferView);
						const Uchar* source = model.buffers.at(bufferView.buffer).data.data() + bufferView.byteOffset + accessor.byteOffset;
						Size sourceStride = accessor.ByteStride(bufferView);
						Size componentByteSize = static_cast<Size>(tinygltf::GetComponentSizeInBytes(static_cast<Uint32>(accessor.componentType)));
						Bool isPlainFloat = accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT;

						for (Size index = 0;index < vertexCount;index++)
						{
							Float* destination = reinterpret_cast<Float*>(&(crister.vertices_[baseVertex + index].*memberPtr));
							const Uchar* element = source + index * sourceStride;
							if (isPlainFloat)
							{
								memcpy(destination, element, componentCount * sizeof(Float));
							}
							else
							{
								for (Size component = 0;component < componentCount;component++)
								{
									destination[component] = decodeComponent(element + component * componentByteSize, accessor.componentType, accessor.normalized);
								}
							}
						}
					};

				fetchAttribute("POSITION", &Vertex::position_, 3);
				fetchAttribute("NORMAL", &Vertex::normal_, 3);
				fetchAttribute("TANGENT", &Vertex::tangent_, 4);
				fetchAttribute("TEXCOORD_0", &Vertex::texcoord_, 2);

				/// [EN] JOINTS_0: joint indices need type-specific handling because glTF allows
				///      UNSIGNED_INT, UNSIGNED_SHORT, or UNSIGNED_BYTE storage.
				/// [JP] JOINTS_0: ジョイントインデックスは glTF が UNSIGNED_INT, UNSIGNED_SHORT,
				///      UNSIGNED_BYTE を許容するため、型別の処理が必要。
				{
					auto it = gltfPrimitive.attributes.find("JOINTS_0");
					if (it != gltfPrimitive.attributes.end())
					{
						const tinygltf::Accessor& accessor = model.accessors.at(it->second);
						const tinygltf::BufferView& bufferView = model.bufferViews.at(accessor.bufferView);
						const Uchar* source = model.buffers.at(bufferView.buffer).data.data() + bufferView.byteOffset + accessor.byteOffset;

						for (Size index = 0;index < vertexCount;index++)
						{
							Vertex& vertex = crister.vertices_[baseVertex + index];
							if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
							{
								const Uint* data = reinterpret_cast<const Uint*>(source + index * accessor.ByteStride(bufferView));
								vertex.joints_ = { data[0],data[1],data[2],data[3] };
							}
							else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
							{
								const Ushort* data = reinterpret_cast<const Ushort*>(source + index * accessor.ByteStride(bufferView));
								vertex.joints_ = { data[0],data[1],data[2],data[3] };
							}
							else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
							{
								const Uchar* data = reinterpret_cast<const Uchar*>(source + index * accessor.ByteStride(bufferView));
								vertex.joints_ = { data[0],data[1],data[2],data[3] };
							}
						}
					}
				}

				/// [EN] WEIGHTS_0: bone weights can be FLOAT, UNSIGNED_SHORT (normalised to 0-65535),
				///      or UNSIGNED_BYTE (normalised to 0-255). Normalise to [0, 1] range.
				/// [JP] WEIGHTS_0: ボーンウェイトは FLOAT, UNSIGNED_SHORT（0-65535 に正規化）,
				///      UNSIGNED_BYTE（0-255 に正規化）がありうる。[0, 1] 範囲に正規化する。
				{
					auto it = gltfPrimitive.attributes.find("WEIGHTS_0");
					if (it != gltfPrimitive.attributes.end())
					{
						const tinygltf::Accessor& accessor = model.accessors.at(it->second);
						const tinygltf::BufferView& bufferView = model.bufferViews.at(accessor.bufferView);
						const Uchar* source = model.buffers.at(bufferView.buffer).data.data() + bufferView.byteOffset + accessor.byteOffset;

						for (Size index = 0;index < vertexCount;index++)
						{
							Vertex& vertex = crister.vertices_[baseVertex + index];
							if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
							{
								const Float* data = reinterpret_cast<const Float*>(source + index * accessor.ByteStride(bufferView));
								vertex.weights_ = Vector4(data[0], data[1], data[2], data[3]);
							}
							else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
							{
								const Ushort* data = reinterpret_cast<const Ushort*>(source + index * accessor.ByteStride(bufferView));
								vertex.weights_ = Vector4(data[0] / 65535.0f, data[1] / 65535.0f, data[2] / 65535.0f, data[3] / 65535.0f);
							}
							else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
							{
								const Uchar* data = reinterpret_cast<const Uchar*>(source + index * accessor.ByteStride(bufferView));
								vertex.weights_ = Vector4(data[0] / 255.0f, data[1] / 255.0f, data[2] / 255.0f, data[3] / 255.0f);
							}
						}
					}
				}

				/**
				* [EN]
				* Morph targets: gltfPrimitive.targets has one entry per morph
				* target, each a name->accessor-index map (only "POSITION" is
				* honoured here — normal/tangent morphing is not supported).
				* Deltas are decoded with the same decodeComponent used for
				* POSITION/NORMAL/TANGENT/TEXCOORD_0 above, so a quantized
				* (KHR_mesh_quantization) morph accessor decodes correctly
				* instead of being misread as FLOAT. Target names come from
				* the common exporter convention mesh.extras.targetNames (a
				* JSON array of strings, in target order, e.g. as written by
				* Blender's glTF exporter); a target without a matching name
				* falls back to "Morph_<index>".
				*
				* ---------------------------------------------------------------------
				*
				* [JP]
				* モーフターゲット: gltfPrimitive.targets はモーフターゲット
				* 1つにつき1エントリで、それぞれが name->アクセサインデックスの
				* map ("POSITION" のみ対応 — 法線/接線のモーフィングは非対応)。
				* デルタは上の POSITION/NORMAL/TANGENT/TEXCOORD_0 と同じ
				* decodeComponent でデコードするため、量子化された
				* (KHR_mesh_quantization) モーフアクセサも FLOAT として誤読
				* されず正しくデコードされる。ターゲット名は一般的な
				* エクスポーター慣例である mesh.extras.targetNames（ターゲット順
				* の文字列 JSON 配列、例えば Blender の glTF エクスポーターが
				* 出力する）から取得する。一致する名前が無いターゲットは
				* "Morph_<index>" にフォールバックする。
				*/
				const tinygltf::Value& targetNames = gltfMesh.extras.Has("targetNames") ? gltfMesh.extras.Get("targetNames") : tinygltf::Value();
				for (Size targetIndex = 0;targetIndex < gltfPrimitive.targets.size();targetIndex++)
				{
					const std::map<std::string, int>& target = gltfPrimitive.targets[targetIndex];
					auto positionIt = target.find("POSITION");
					if (positionIt == target.end())
					{
						continue;
					}

					Morph& morph = subMesh.morphs_.emplace_back();
					morph.name_ = (targetNames.IsArray() && targetIndex < targetNames.ArrayLen() && targetNames.Get(static_cast<Int>(targetIndex)).IsString())
						? targetNames.Get(static_cast<Int>(targetIndex)).Get<std::string>()
						: "Morph_" + std::to_string(targetIndex);
					morph.positionDeltas_.resize(vertexCount);

					const tinygltf::Accessor& accessor = model.accessors.at(positionIt->second);
					const tinygltf::BufferView& bufferView = model.bufferViews.at(accessor.bufferView);
					const Uchar* source = model.buffers.at(bufferView.buffer).data.data() + bufferView.byteOffset + accessor.byteOffset;
					Size sourceStride = accessor.ByteStride(bufferView);
					Size componentByteSize = static_cast<Size>(tinygltf::GetComponentSizeInBytes(static_cast<Uint32>(accessor.componentType)));
					Bool isPlainFloat = accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT;

					for (Size index = 0;index < vertexCount;index++)
					{
						const Uchar* element = source + index * sourceStride;
						if (isPlainFloat)
						{
							Float delta[3];
							memcpy(delta, element, sizeof(delta));
							morph.positionDeltas_[index] = Vector3(delta[0], delta[1], delta[2]);
						}
						else
						{
							morph.positionDeltas_[index] = Vector3(
								decodeComponent(element + 0 * componentByteSize, accessor.componentType, accessor.normalized),
								decodeComponent(element + 1 * componentByteSize, accessor.componentType, accessor.normalized),
								decodeComponent(element + 2 * componentByteSize, accessor.componentType, accessor.normalized));
						}
					}
				}

				/// [EN] Record where this SubMesh's indices begin in vertexIndices_.
				/// [JP] この SubMesh のインデックスが vertexIndices_ 内のどこから始まるかを記録する。
				subMesh.indexOffset_ = static_cast<Uint32>(crister.vertexIndices_.size());

				if (gltfPrimitive.indices > -1)
				{
					/// [EN] Indexed primitive: read the index buffer with type-specific handling.
					///      glTF index buffers can be UNSIGNED_INT, UNSIGNED_SHORT, or UNSIGNED_BYTE.
					///      Each index is offset by vertexOffset to produce a global index into crister.vertices_.
					/// [JP] インデックス付きプリミティブ: 型別にインデックスバッファを読み取る。
					///      glTF インデックスバッファは UNSIGNED_INT, UNSIGNED_SHORT, UNSIGNED_BYTE がありうる。
					///      各インデックスに vertexOffset を加え、crister.vertices_ へのグローバルインデックスにする。
					const tinygltf::Accessor& accessor = model.accessors.at(gltfPrimitive.indices);
					const tinygltf::BufferView& bufferView = model.bufferViews.at(accessor.bufferView);
					const Uchar* source = model.buffers.at(bufferView.buffer).data.data() + bufferView.byteOffset + accessor.byteOffset;

					Size indexBase = crister.vertexIndices_.size();
					crister.vertexIndices_.resize(indexBase + accessor.count);

					for (Size index = 0;index < accessor.count;index++)
					{
						Uint32 vertexIndex = 0;
						if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
						{
							vertexIndex = *reinterpret_cast<const Uint32*>(source + index * sizeof(Uint32));
						}
						else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
						{
							vertexIndex = *reinterpret_cast<const Ushort*>(source + index * sizeof(Ushort));
						}
						else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
						{
							vertexIndex = source[index];
						}
						crister.vertexIndices_[indexBase + index] = vertexOffset + vertexIndex;
					}
				}
				else
				{
					/// [EN] Non-indexed primitive: generate sequential indices (0, 1, 2, ...).
					///      Some glTF files omit the index buffer entirely when every vertex
					///      is unique (no sharing). We create identity indices so the rest of
					///      the pipeline can assume an index buffer always exists.
					/// [JP] 非インデックスプリミティブ: 連番インデックス (0, 1, 2, ...) を生成する。
					///      一部の glTF ファイルは全頂点がユニーク（共有なし）の場合、インデックス
					///      バッファを完全に省略する。パイプラインの残りがインデックスバッファの
					///      存在を仮定できるよう、恒等インデックスを作成する。
					Size indexBase = crister.vertexIndices_.size();
					crister.vertexIndices_.resize(indexBase + vertexCount);
					for (Size index = 0;index < vertexCount;index++)
					{
						crister.vertexIndices_[indexBase + index] = vertexOffset + static_cast<Uint32>(index);
					}
				}

				subMesh.indexCount_ = static_cast<Uint32>(crister.vertexIndices_.size()) - subMesh.indexOffset_;
			}
		}
	}

	/**
	* [EN]
	* Extracts PBR material data from the glTF model.
	*
	* Each material includes:
	*   - Base color, metallic, roughness (PBR metallic-roughness model)
	*   - Emissive factor
	*   - Alpha mode (OPAQUE=0, MASK=1, BLEND=2) and cutoff
	*   - Texture indices for base color, normal, metallic-roughness,
	*     occlusion, and emissive maps (0xFFFFFFFF if not present)
	*
	* Note: glTF "texture" objects reference "image" objects via a source
	* index. We resolve to the image index directly so that the texture
	* indices match crister.bitmaps_ (which is populated from images).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF モデルから PBR マテリアルデータを抽出する。
	*
	* 各マテリアルに含まれるもの:
	*   - ベースカラー、メタリック、ラフネス（PBR metallic-roughness モデル）
	*   - エミッシブファクター
	*   - アルファモード（OPAQUE=0, MASK=1, BLEND=2）とカットオフ
	*   - ベースカラー、法線、メタリックラフネス、オクルージョン、エミッシブ各マップの
	*     テクスチャインデックス（存在しなければ 0xFFFFFFFF）
	*
	* 注: glTF の "texture" オブジェクトは source インデックスを介して
	* "image" オブジェクトを参照する。crister.bitmaps_（image から構築）と
	* 一致するよう、image インデックスに直接解決する。
	*/
	void ModelLoader::FetchMaterials(const tinygltf::Model& model, Crister& crister)
	{
		for (const tinygltf::Material& gltfMaterial : model.materials)
		{
			Material& material = crister.materials_.emplace_back();

			const auto& pbr = gltfMaterial.pbrMetallicRoughness;
			material.baseColor_ = Color(static_cast<Float>(pbr.baseColorFactor[0]), static_cast<Float>(pbr.baseColorFactor[1]), static_cast<Float>(pbr.baseColorFactor[2]), static_cast<Float>(pbr.baseColorFactor[3]));
			material.metallic_ = static_cast<Float>(pbr.metallicFactor);
			material.roughness_ = static_cast<Float>(pbr.roughnessFactor);

			material.emissiveFactor_[0] = static_cast<Float>(gltfMaterial.emissiveFactor[0]);
			material.emissiveFactor_[1] = static_cast<Float>(gltfMaterial.emissiveFactor[1]);
			material.emissiveFactor_[2] = static_cast<Float>(gltfMaterial.emissiveFactor[2]);

			material.alphaMode_ = gltfMaterial.alphaMode == "OPAQUE" ? 0 : gltfMaterial.alphaMode == "MASK" ? 1 : 2;
			material.alphaCutoff_ = static_cast<Float>(gltfMaterial.alphaCutoff);
			material.doubleSided_ = gltfMaterial.doubleSided ? 1 : 0;

			if (pbr.baseColorTexture.index >= 0)
			{
				material.baseColorTextureIndex_ = model.textures[pbr.baseColorTexture.index].source;
			}
			if (gltfMaterial.normalTexture.index >= 0)
			{
				material.normalTextureIndex_ = model.textures[gltfMaterial.normalTexture.index].source;
			}
			if (pbr.metallicRoughnessTexture.index >= 0)
			{
				material.metallicRoughnessTextureIndex_ = model.textures[pbr.metallicRoughnessTexture.index].source;
			}
			if (gltfMaterial.occlusionTexture.index >= 0)
			{
				material.occlusionTextureIndex_ = model.textures[gltfMaterial.occlusionTexture.index].source;
			}
			if (gltfMaterial.emissiveTexture.index >= 0)
			{
				material.emissiveTextureIndex_ = model.textures[gltfMaterial.emissiveTexture.index].source;
			}

			/// [JP] KHR_materials_* 拡張を読む。フィールドが無ければ struct の
			///      デフォルト（中立値）のまま。テクスチャは { "index": N } を
			///      model.textures[N].source で image インデックスに解決する。
			auto extFloat = [](const tinygltf::Value& value, const Char* key, Float fallback) -> Float
			{
				return value.Has(key) ? static_cast<Float>(value.Get(key).GetNumberAsDouble()) : fallback;
			};
			auto extColor3 = [](const tinygltf::Value& value, const Char* key, Float* destination)
			{
				if (value.Has(key))
				{
					const tinygltf::Value& array = value.Get(key);
					if (array.IsArray() && array.ArrayLen() >= 3)
					{
						destination[0] = static_cast<Float>(array.Get(0).GetNumberAsDouble());
						destination[1] = static_cast<Float>(array.Get(1).GetNumberAsDouble());
						destination[2] = static_cast<Float>(array.Get(2).GetNumberAsDouble());
					}
				}
			};
			auto extTexture = [&model](const tinygltf::Value& value, const Char* key) -> Uint32
			{
				if (value.Has(key))
				{
					const tinygltf::Value& info = value.Get(key);
					if (info.Has("index"))
					{
						const Int textureIndex = static_cast<Int>(info.Get("index").GetNumberAsDouble());
						if (textureIndex >= 0)
						{
							return static_cast<Uint32>(model.textures[textureIndex].source);
						}
					}
				}
				return 0xFFFFFFFF;
			};

			const auto& extensions = gltfMaterial.extensions;

			if (auto it = extensions.find("KHR_materials_emissive_strength"); it != extensions.end())
			{
				material.khr_.emissiveStrength_.emissiveStrength_ = extFloat(it->second, "emissiveStrength", 1.0f);
			}

			if (auto it = extensions.find("KHR_materials_ior"); it != extensions.end())
			{
				material.khr_.ior_.ior_ = extFloat(it->second, "ior", 1.5f);
			}

			if (auto it = extensions.find("KHR_materials_specular"); it != extensions.end())
			{
				auto& specular = material.khr_.specular_;
				specular.specularFactor_ = extFloat(it->second, "specularFactor", 1.0f);
				extColor3(it->second, "specularColorFactor", specular.specularColorFactor_);
				specular.specularTextureIndex_ = extTexture(it->second, "specularTexture");
				specular.specularColorTextureIndex_ = extTexture(it->second, "specularColorTexture");
			}

			if (auto it = extensions.find("KHR_materials_clearcoat"); it != extensions.end())
			{
				auto& clearCoat = material.khr_.clearCoat_;
				clearCoat.clearCoatFactor_ = extFloat(it->second, "clearcoatFactor", 0.0f);
				clearCoat.clearCoatRoughnessFactor_ = extFloat(it->second, "clearcoatRoughnessFactor", 0.0f);
				clearCoat.clearCoatTextureIndex_ = extTexture(it->second, "clearcoatTexture");
				clearCoat.clearCoatRoughnessTextureIndex_ = extTexture(it->second, "clearcoatRoughnessTexture");
				clearCoat.clearCoatNormalTextureIndex_ = extTexture(it->second, "clearcoatNormalTexture");
			}

			if (auto it = extensions.find("KHR_materials_transmission"); it != extensions.end())
			{
				auto& transmission = material.khr_.transmission_;
				transmission.transmissionFactor_ = extFloat(it->second, "transmissionFactor", 0.0f);
				transmission.transmissionTextureIndex_ = extTexture(it->second, "transmissionTexture");

				/// [JP] KHR_materials_transmission の仕様上、拡張に対応する実装は
				///      alphaMode の宣言(BLENDなど)を無視して常にOPAQUE扱いする
				///      決まりになっている(BLEND宣言は拡張未対応ビューア向けの
				///      フォールバックに過ぎない)。ここで上書きしないと
				///      ModelRenderer.cpp の alphaMode_ != 2 判定で OIT
				///      (未対応のunlitアルファブレンド)側へ回ってしまい、
				///      Refraction/KHR拡張のライティングが一切乗らず真っ黒になる。
				if (transmission.transmissionFactor_ > 0.0f)
				{
					material.alphaMode_ = 0;
				}
			}

			if (auto it = extensions.find("KHR_materials_volume"); it != extensions.end())
			{
				auto& volume = material.khr_.volume_;
				volume.thicknessFactor_ = extFloat(it->second, "thicknessFactor", 0.0f);
				volume.attenuationDistance_ = extFloat(it->second, "attenuationDistance", FLT_MAX);
				extColor3(it->second, "attenuationColor", volume.attenuationColor_);
				volume.thicknessTextureIndex_ = extTexture(it->second, "thicknessTexture");
			}

			if (auto it = extensions.find("KHR_materials_sheen"); it != extensions.end())
			{
				auto& sheen = material.khr_.sheen_;
				extColor3(it->second, "sheenColorFactor", sheen.sheenColorFactor_);
				sheen.sheenRoughnessFactor_ = extFloat(it->second, "sheenRoughnessFactor", 0.0f);
				sheen.sheenColorTextureIndex_ = extTexture(it->second, "sheenColorTexture");
				sheen.sheenRoughnessTextureIndex_ = extTexture(it->second, "sheenRoughnessTexture");
			}

			if (auto it = extensions.find("KHR_materials_iridescence"); it != extensions.end())
			{
				auto& iridescence = material.khr_.iridescence_;
				iridescence.iridescenceFactor_ = extFloat(it->second, "iridescenceFactor", 0.0f);
				iridescence.iridescenceIor_ = extFloat(it->second, "iridescenceIor", 1.3f);
				iridescence.iridescenceThicknessMinimum_ = extFloat(it->second, "iridescenceThicknessMinimum", 100.0f);
				iridescence.iridescenceThicknessMaximum_ = extFloat(it->second, "iridescenceThicknessMaximum", 400.0f);
				iridescence.iridescenceTextureIndex_ = extTexture(it->second, "iridescenceTexture");
				iridescence.iridescenceThicknessTextureIndex_ = extTexture(it->second, "iridescenceThicknessTexture");
			}

			if (auto it = extensions.find("KHR_materials_anisotropy"); it != extensions.end())
			{
				auto& anisotropy = material.khr_.anisotropy_;
				anisotropy.anisotropyStrength_ = extFloat(it->second, "anisotropyStrength", 0.0f);
				anisotropy.anisotropyRotation_ = extFloat(it->second, "anisotropyRotation", 0.0f);
				anisotropy.anisotropyTextureIndex_ = extTexture(it->second, "anisotropyTexture");
			}

			if (extensions.count("KHR_materials_unlit"))
			{
				material.khr_.unlit_.unlit_ = 1;
			}
		}
	}

	/**
	* [EN]
	* Extracts skeleton (skin) data from the glTF model.
	*
	* Each skin contains:
	*   - A list of joint node indices
	*   - An array of inverse bind matrices (one per joint)
	*
	* The inverse bind matrix transforms a vertex from model space into
	* the local space of the bone. At runtime, the final bone matrix is:
	*   finalMatrix = inverseBindMatrix * jointGlobalTransform
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF モデルからスケルトン（スキン）データを抽出する。
	*
	* 各スキンに含まれるもの:
	*   - ジョイントノードインデックスのリスト
	*   - 逆バインド行列の配列（ジョイントごとに 1 つ）
	*
	* 逆バインド行列は頂点をモデル空間からボーンのローカル空間に変換する。
	* ランタイムでの最終ボーン行列は:
	*   finalMatrix = inverseBindMatrix * jointGlobalTransform
	*/
	void ModelLoader::FetchSkins(const tinygltf::Model& model, Crister& crister)
	{
		for (const tinygltf::Skin& gltfSkin : model.skins)
		{
			Skin& skin = crister.skins_.emplace_back();
			skin.joints_ = gltfSkin.joints;

			if (gltfSkin.inverseBindMatrices >= 0)
			{
				const tinygltf::Accessor& accessor = model.accessors.at(gltfSkin.inverseBindMatrices);
				const tinygltf::BufferView& bufferView = model.bufferViews.at(accessor.bufferView);
				skin.inverseBindMatrices_.resize(accessor.count);

				/// [EN] Raw copy is correct: glTF's column-major storage of its column-vector
				///      matrices coincides byte-for-byte with the engine's row-major storage of
				///      the equivalent row-vector matrices. Verified against an independent
				///      reference implementation (palette error < 1e-5). Do NOT transpose here.
				/// [JP] 生コピーで正しい: glTF の列ベクトル規約行列の column-major 格納は、
				///      等価な行ベクトル規約行列の row-major 格納とバイト単位で一致する。
				///      独立した参照実装と照合済み（パレット誤差 < 1e-5）。ここで転置しないこと。
				memcpy(skin.inverseBindMatrices_.data(), model.buffers.at(bufferView.buffer).data.data() + bufferView.byteOffset + accessor.byteOffset, accessor.count * sizeof(Matrix));
			}
		}
	}

	/**
	* [EN]
	* Resolves SubMesh → skin association.
	*
	* FetchMeshes flattens all primitives of all glTF meshes into
	* crister.subMeshes_ in order, so the SubMesh range of glTF mesh M is
	* [prefix(M), prefix(M) + primitiveCount(M)). A mesh is skinned when a
	* node references it together with a skin; per the glTF spec, the
	* transform of such a node is ignored and only joint transforms apply.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* SubMesh → スキンの対応を解決する。
	*
	* FetchMeshes は全 glTF メッシュの全プリミティブを順番に
	* crister.subMeshes_ へフラット化するため、glTF メッシュ M の SubMesh
	* 範囲は [prefix(M), prefix(M) + primitiveCount(M))。メッシュはノードが
	* skin と一緒に参照しているときスキンドとなる。glTF 仕様では、そのような
	* ノードのトランスフォームは無視され、ジョイントのトランスフォームのみが
	* 適用される。
	*/
	void ModelLoader::ResolveSubMeshSkins(const tinygltf::Model& model, Crister& crister)
	{
		/// [EN] Prefix sum of primitive counts = SubMesh offset per glTF mesh.
		/// [JP] プリミティブ数の累積和 = glTF メッシュごとの SubMesh オフセット。
		DynamicArray<Uint32> subMeshOffsets;
		subMeshOffsets.reserve(model.meshes.size());
		Uint32 offset = 0;
		for (const tinygltf::Mesh& gltfMesh : model.meshes)
		{
			subMeshOffsets.push_back(offset);
			offset += static_cast<Uint32>(gltfMesh.primitives.size());
		}

		for (const tinygltf::Node& gltfNode : model.nodes)
		{
			if (gltfNode.mesh < 0 || gltfNode.skin < 0)
			{
				continue;
			}

			Uint32 begin = subMeshOffsets[gltfNode.mesh];
			Uint32 count = static_cast<Uint32>(model.meshes[gltfNode.mesh].primitives.size());
			for (Uint32 index = 0; index < count; index++)
			{
				crister.subMeshes_[begin + index].skinIndex_ = gltfNode.skin;
			}
		}
	}

	/**
	* [EN]
	* Copies raw texture image data from the glTF model into Crister's
	* texture array. This is a data packing step only — actual GPU texture
	* creation happens later in Crister::Upload.
	*
	* tinygltf has already decoded the image data (PNG/JPEG → raw pixels)
	* by the time we access it, so we just copy the decoded buffer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF モデルの生テクスチャ画像データを Crister のテクスチャ配列にコピーする。
	* これはデータを詰めるだけのステップであり、実際の GPU テクスチャ作成は
	* Crister::Upload で行う。
	*
	* tinygltf はアクセス時点で既に画像データをデコード済み
	* （PNG/JPEG → 生ピクセル）なので、デコード済みバッファをコピーするだけ。
	*/
	void ModelLoader::FetchTexture(const tinygltf::Model& model, Crister& crister)
	{
		for (const tinygltf::Image& gltfImage : model.images)
		{
			Bitmap& texture = crister.bitmaps_.emplace_back();
			texture.name_ = gltfImage.name;
			texture.width_ = gltfImage.width;
			texture.height_ = gltfImage.height;
			texture.component_ = gltfImage.component;
			texture.bits_ = gltfImage.bits;
			texture.mimeType_ = gltfImage.mimeType;
			texture.cacheData_ = gltfImage.image;
		}
	}

	/**
	* [EN]
	* Generates meshlets and a multi-level LOD hierarchy for all SubMeshes.
	*
	* This is the core of the Mesh Shader rendering pipeline preparation:
	*
	* ┌────────────────────────────────────────────────────────────────────┐
	* │  MESHLET: A small group of triangles processed by one GPU thread  │
	* │           group (workgroup) in the Mesh Shader.                   │
	* │                                                                   │
	* │  Constraints:                                                     │
	* │    - Max 64 unique vertices per meshlet                           │
	* │    - Max 124 triangles per meshlet                                │
	* │                                                                   │
	* │  Why 64 / 124?                                                    │
	* │    - 64 vertices = the typical thread count in an MS workgroup    │
	* │    - 124 triangles × 3 = 372 indices → fits in shared memory	   ｜
	* │    - These limits come from D3D12 Mesh Shader best practices      │
	* ├────────────────────────────────────────────────────────────────────┤
	* │  MESHLET BOUND: Per-meshlet bounding sphere + normal cone for     │
	* │                 culling in the Amplification Shader (AS).         │
	* │                                                                   │
	* │  The AS can skip entire meshlets that are:                        │
	* │    - Outside the view frustum (bounding sphere test)              │
	* │    - Facing away from the camera (normal cone test)               │
	* ├────────────────────────────────────────────────────────────────────┤
	* │  LOD (Level of Detail): Multiple simplified versions of the mesh. │
	* │                                                                   │
	* │  LOD 0 = original (full detail)                                   │
	* │  LOD 1 = ~50% triangles                                           │
	* │  LOD 2 = ~25% triangles                                           │
	* │  ...                                                              │
	* │  LOD N = minimum triangles (< 4 stops the chain)                  │
	* │                                                                   │
	* │  Each LOD is a "Cluster" containing its own meshlets.             │
	* │  The AS selects the appropriate LOD based on screen-space size.   │
	* │                                                                   │
	* │  lodError is CUMULATIVE (not per-level delta) so the AS can       │
	* │  compare it directly against the screen-space error threshold.    │
	* └────────────────────────────────────────────────────────────────────┘
	*
	* Data flow:
	*
	*   vertexIndices_  →  per-SubMesh slice
	*                          │
	*                          ▼
	*                   LOD 0: buildMeshletsFromIndices → computeBounds → Cluster
	*                          │
	*                          ▼
	*                   QEM Simplify (50% reduction)
	*                          │
	*                          ▼
	*                   LOD 1: buildMeshletsFromIndices → computeBounds → Cluster
	*                          │
	*                          ▼
	*                   QEM Simplify (50% reduction)
	*                          │
	*                         ...
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全 SubMesh に対してメシュレットとマルチレベル LOD 階層を生成する。
	*
	* Mesh Shader レンダリングパイプライン準備のコア部分:
	*
	* ┌────────────────────────────────────────────────────────────────────┐
	* │  メシュレット: GPU の 1 スレッドグループ（ワークグループ）で処理   │
	* │               される小さな三角形グループ。                         │
	* │                                                                    │
	* │  制約:                                                             │
	* │    - メシュレットあたり最大 64 固有頂点                            │
	* │    - メシュレットあたり最大 124 三角形                             │
	* │                                                                    │
	* │  なぜ 64 / 124 か？                                                │
	* │    - 64 頂点 = MS ワークグループの典型的なスレッド数               │
	* │    - 124 三角形 × 3 = 372 インデックス → 共有メモリに収まる      │
	* │    - これらの制限は D3D12 Mesh Shader のベストプラクティスによる   │
	* ├────────────────────────────────────────────────────────────────────┤
	* │  メシュレットバウンド: Amplification Shader (AS) でのカリング用    │
	* │                       メシュレットごとの包囲球 + 法線コーン。      │
	* │                                                                    │
	* │  AS は以下のメシュレットを丸ごとスキップできる:                    │
	* │    - ビューフラスタムの外（包囲球テスト）                          │
	* │    - カメラに背を向けている（法線コーンテスト）                    │
	* ├────────────────────────────────────────────────────────────────────┤
	* │  LOD（詳細度レベル）: メッシュの複数の簡略化バージョン。           │
	* │                                                                    │
	* │  LOD 0 = オリジナル（最大詳細度）                                  │
	* │  LOD 1 = 三角形 ~50%                                               │
	* │  LOD 2 = 三角形 ~25%                                               │
	* │  ...                                                               │
	* │  LOD N = 最小三角形数（4 未満でチェーン停止）                      │
	* │                                                                    │
	* │  各 LOD は独自のメシュレットを含む「Cluster」である。              │
	* │  AS が画面空間サイズに基づいて適切な LOD を選択する。              │
	* │                                                                    │
	* │  lodError は累積（レベルごとのデルタではない）なので、AS が        │
	* │  画面空間エラー閾値と直接比較できる。                              │
	* └────────────────────────────────────────────────────────────────────┘
	*
	* データフロー:
	*
	*   vertexIndices_  →  SubMesh ごとのスライス
	*                          │
	*                          ▼
	*                   LOD 0: buildMeshletsFromIndices → computeBounds → Cluster
	*                          │
	*                          ▼
	*                   QEM Simplify（50% 削減）
	*                          │
	*                          ▼
	*                   LOD 1: buildMeshletsFromIndices → computeBounds → Cluster
	*                          │
	*                          ▼
	*                   QEM Simplify（50% 削減）
	*                          │
	*                         ...
	*/
	void ModelLoader::BuildMeshlets(Crister& crister)
	{
		/// [EN] Mesh Shader workgroup limits. These match D3D12 best practices.
		/// [JP] Mesh Shader ワークグループの制限。D3D12 ベストプラクティスに準拠。
		constexpr Uint32 maxVertices = 64;
		constexpr Uint32 maxTriangles = 124;

		/// [EN] Maximum LOD depth. 24 levels at 50% reduction per level gives
		///      a theoretical reduction of 2^24 ≈ 16 million : 1.
		/// [JP] 最大 LOD 深度。レベルごと 50% 削減で 24 レベルなら
		///      理論上 2^24 ≈ 1600 万 : 1 の削減。
		constexpr Uint32 maxLodLevels = 24;

		/// [EN] Move the original global index buffer out — we rebuild it from scratch
		///      as meshlet vertex indices (vertexIndices_) and primitive indices (primitiveIndices_).
		/// [JP] 元のグローバルインデックスバッファを退避 — メシュレットの頂点インデックス
		///      (vertexIndices_) とプリミティブインデックス (primitiveIndices_) として一から再構築する。
		DynamicArray<Uint32> originalIndices = std::move(crister.vertexIndices_);
		crister.vertexIndices_.clear();
		crister.primitiveIndices_.clear();
		crister.meshlets_.clear();
		crister.meshletBounds_.clear();
		crister.clusters_.clear();

		/**
		* [EN]
		* Builds meshlets from a flat triangle index buffer.
		*
		* Algorithm (greedy bin-packing):
		*   For each triangle:
		*     1. Count how many of its 3 vertices are NEW to the current meshlet.
		*     2. If adding them would exceed maxVertices, or the triangle count
		*        would exceed maxTriangles, flush the current meshlet and start
		*        a new one.
		*     3. For each vertex of the triangle:
		*        - If new, assign it a local index and append its global index
		*          to vertexIndices_.
		*        - Record the local index in primitiveIndices_ (3 per triangle).
		*
		* The two-level indirection works like this:
		*
		*   primitiveIndices_[i] = local index (0..63) within the meshlet
		*   vertexIndices_[meshlet.vertexOffset_ + local] = global vertex index
		*
		* This allows the Mesh Shader to load only the vertices actually used
		* by the meshlet, with compact 8-bit primitive indices.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* フラットな三角形インデックスバッファからメシュレットを構築する。
		*
		* アルゴリズム（貪欲ビンパッキング）:
		*   各三角形について:
		*     1. その 3 頂点のうち現在のメシュレットにとって新しい頂点数を数える。
		*     2. 追加で maxVertices を超える、または三角形数が maxTriangles を超える
		*        なら、現在のメシュレットをフラッシュして新しいものを開始する。
		*     3. 三角形の各頂点について:
		*        - 新しければ、ローカルインデックスを割り当て、グローバルインデックスを
		*          vertexIndices_ に追加する。
		*        - ローカルインデックスを primitiveIndices_ に記録する（三角形ごとに 3 つ）。
		*
		* 二段階の間接参照は以下のように機能する:
		*
		*   primitiveIndices_[i] = メシュレット内のローカルインデックス (0..63)
		*   vertexIndices_[meshlet.vertexOffset_ + local] = グローバル頂点インデックス
		*
		* これにより Mesh Shader はメシュレットが実際に使う頂点のみをロードでき、
		* コンパクトな 8 ビットプリミティブインデックスが使える。
		*/
		auto buildMeshletsFromIndices = [&](const Uint32* indices, Size indexCount) -> void
			{

				Uint32 currentVertexCount = 0;
				Uint32 currentTriangleCount = 0;

				/// [EN] Maps global vertex index → local index (0..63) within the current meshlet.
				/// [JP] グローバル頂点インデックス → 現在のメシュレット内のローカルインデックス (0..63) のマップ。
				std::unordered_map<Uint32, Uint8> localVertexMap;

				Uint32 vertexOffset = static_cast<Uint32>(crister.vertexIndices_.size());
				Uint32 triangleOffset = static_cast<Uint32>(crister.primitiveIndices_.size());

				/// [EN] Finalises the current meshlet and resets state for the next one.
				/// [JP] 現在のメシュレットを確定し、次のメシュレットのために状態をリセットする。
				auto flushMeshlet = [&]()
					{
						if (currentTriangleCount == 0)
						{
							return;
						}

						Meshlet& meshlet = crister.meshlets_.emplace_back();
						meshlet.vertexOffset_ = vertexOffset;
						meshlet.triangleOffset_ = triangleOffset;
						meshlet.vertexCount_ = currentVertexCount;
						meshlet.triangleCount_ = currentTriangleCount;

						vertexOffset = static_cast<Uint32>(crister.vertexIndices_.size());
						triangleOffset = static_cast<Uint32>(crister.primitiveIndices_.size());
						currentVertexCount = 0;
						currentTriangleCount = 0;
						localVertexMap.clear();
					};

				Size triangleCount = indexCount / 3;
				for (Size triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
				{
					Uint32 triangle[3] = { indices[triangleIndex * 3], indices[triangleIndex * 3 + 1], indices[triangleIndex * 3 + 2] };

					/// [EN] Count how many of this triangle's vertices are not yet in the current meshlet.
					/// [JP] この三角形の頂点のうち、現在のメシュレットにまだ入っていないものを数える。
					Uint32 newVerts = 0;
					for (Int index = 0; index < 3; index++)
					{
						if (localVertexMap.find(triangle[index]) == localVertexMap.end())
						{
							newVerts++;
						}
					}

					/// [EN] If this triangle would overflow the meshlet, flush and start fresh.
					/// [JP] この三角形でメシュレットがあふれるなら、フラッシュして新しく始める。
					if (currentVertexCount + newVerts > maxVertices || currentTriangleCount + 1 > maxTriangles)
					{
						flushMeshlet();
					}

					/// [EN] Add each vertex to the meshlet, assigning a new local index if needed.
					/// [JP] 各頂点をメシュレットに追加し、必要なら新しいローカルインデックスを割り当てる。
					Uint8 localIndices[3];
					for (Int index = 0; index < 3; index++)
					{
						auto it = localVertexMap.find(triangle[index]);
						if (it == localVertexMap.end())
						{
							Uint8 localIndex = static_cast<Uint8>(currentVertexCount++);
							localVertexMap[triangle[index]] = localIndex;
							crister.vertexIndices_.push_back(triangle[index]);
							localIndices[index] = localIndex;
						}
						else
						{
							localIndices[index] = it->second;
						}
					}

					/// [EN] Append the 3 local indices as primitive (triangle) indices.
					/// [JP] 3 つのローカルインデックスをプリミティブ（三角形）インデックスとして追加する。
					crister.primitiveIndices_.push_back(localIndices[0]);
					crister.primitiveIndices_.push_back(localIndices[1]);
					crister.primitiveIndices_.push_back(localIndices[2]);
					currentTriangleCount++;
				}

				flushMeshlet();
			};

		/**
		* [EN]
		* Computes the bounding sphere and normal cone for a range of meshlets.
		*
		* Bounding sphere:
		*   - Center = average of all vertex positions in the meshlet.
		*   - Radius = max distance from center to any vertex.
		*   Used by the AS for frustum culling (reject meshlets entirely
		*   outside the camera frustum).
		*
		* Normal cone:
		*   - coneAxis = average of all face normals in the meshlet.
		*   - coneCutoff = minimum dot product between any face normal and
		*     the cone axis. If coneCutoff > 0, all faces point roughly
		*     the same direction, and the AS can do backface culling:
		*     if dot(viewDir, coneAxis) < -coneCutoff, the entire meshlet
		*     faces away from the camera.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* メシュレット範囲に対して包囲球と法線コーンを計算する。
		*
		* 包囲球:
		*   - 中心 = メシュレット内の全頂点位置の平均。
		*   - 半径 = 中心からいずれかの頂点までの最大距離。
		*   AS がフラスタムカリングに使用する（カメラフラスタムの完全に
		*   外にあるメシュレットを棄却）。
		*
		* 法線コーン:
		*   - coneAxis = メシュレット内の全面法線の平均。
		*   - coneCutoff = いずれかの面法線とコーン軸の最小内積。
		*     coneCutoff > 0 なら全面がほぼ同じ方向を向いており、
		*     AS がバックフェイスカリングを行える:
		*     dot(viewDirection, coneAxis) < -coneCutoff ならメシュレット全体が
		*     カメラに背を向けている。
		*/
		auto computeBounds = [&](Uint32 meshletBegin, Uint32 meshletEnd)
			{
				for (Uint32 meshletIndex = meshletBegin; meshletIndex < meshletEnd; meshletIndex++)
				{
					const Meshlet& meshlet = crister.meshlets_[meshletIndex];
					MeshletBound& bound = crister.meshletBounds_.emplace_back();

					/// [EN] Step 1: Compute bounding sphere.
					/// [JP] ステップ 1: 包囲球を計算する。

					/// [EN] Compute the centroid (average position) of all vertices in this meshlet.
					/// [JP] このメシュレットの全頂点の重心（平均位置）を計算する。
					Vector3 center = Vector3::Zero;
					for (Uint32 index = 0; index < meshlet.vertexCount_; index++)
					{
						Uint32 vertexIndex = crister.vertexIndices_[meshlet.vertexOffset_ + index];
						center += crister.vertices_[vertexIndex].position_;
					}
					center /= static_cast<Float>(meshlet.vertexCount_);

					/// [EN] Find the maximum squared distance from any vertex to the centroid.
					/// [JP] いずれかの頂点から重心までの最大二乗距離を求める。
					Float maxDistance2 = 0;
					for (Uint32 index = 0; index < meshlet.vertexCount_; index++)
					{
						Uint32 vertexIndex = crister.vertexIndices_[meshlet.vertexOffset_ + index];
						Vector3 difference = crister.vertices_[vertexIndex].position_ - center;
						Float distance2 = difference.Dot(difference);
						if (distance2 > maxDistance2)
						{
							maxDistance2 = distance2;
						}
					}

					bound.center_ = center;
					bound.radius_ = std::sqrt(maxDistance2);

					/// [EN] Step 2: Compute normal cone.
					/// [JP] ステップ 2: 法線コーンを計算する。

					/// [EN] Compute the average face normal across all triangles in the meshlet.
					/// [JP] メシュレット内の全三角形にわたる平均面法線を計算する。
					Vector3 avgNormal = Vector3::Zero;
					Uint32 faceCount = 0;

					for (Uint32 triangle = 0; triangle < meshlet.triangleCount_; triangle++)
					{
						Uint32 base = meshlet.triangleOffset_ + triangle * 3;

						/// [EN] Resolve the 2-level indirection: primitiveIndices_ → vertexIndices_ → global vertex.
						/// [JP] 2 段階の間接参照を解決: primitiveIndices_ → vertexIndices_ → グローバル頂点。
						Uint32 gi0 = crister.vertexIndices_[meshlet.vertexOffset_ + crister.primitiveIndices_[base]];
						Uint32 gi1 = crister.vertexIndices_[meshlet.vertexOffset_ + crister.primitiveIndices_[base + 1]];
						Uint32 gi2 = crister.vertexIndices_[meshlet.vertexOffset_ + crister.primitiveIndices_[base + 2]];

						Vector3 p0 = crister.vertices_[gi0].position_;
						Vector3 p1 = crister.vertices_[gi1].position_;
						Vector3 p2 = crister.vertices_[gi2].position_;

						Vector3 e1 = p1 - p0;
						Vector3 e2 = p2 - p0;
						Vector3 n = e2.Cross(e1);
						Float length = n.Length();
						if (length < 1e-8f)
						{
							continue;
						}
						n /= length;

						avgNormal += n;
						faceCount++;
					}

					if (faceCount > 0)
					{
						Float length = avgNormal.Length();
						if (length > 1e-8f)
						{
							avgNormal /= length;
							bound.coneAxis_ = avgNormal;

							/// [EN] Find the tightest cone that contains all face normals:
							///      coneCutoff = min dot(faceNormal, avgNormal) over all faces.
							///      If this is > 0, all faces are within a hemisphere centered on avgNormal.
							/// [JP] 全面法線を包含する最小のコーンを求める:
							///      coneCutoff = 全面について min dot(faceNormal, avgNormal)。
							///      これが > 0 なら全面が avgNormal 中心の半球内にある。
							Float minDot = 1.0f;
							for (Uint32 triangle = 0; triangle < meshlet.triangleCount_; triangle++)
							{
								Uint32 base = meshlet.triangleOffset_ + triangle * 3;
								Uint32 gi0 = crister.vertexIndices_[meshlet.vertexOffset_ + crister.primitiveIndices_[base]];
								Uint32 gi1 = crister.vertexIndices_[meshlet.vertexOffset_ + crister.primitiveIndices_[base + 1]];
								Uint32 gi2 = crister.vertexIndices_[meshlet.vertexOffset_ + crister.primitiveIndices_[base + 2]];

								Vector3 p0 = crister.vertices_[gi0].position_;
								Vector3 p1 = crister.vertices_[gi1].position_;
								Vector3 p2 = crister.vertices_[gi2].position_;

								Vector3 e1 = p1 - p0;
								Vector3 e2 = p2 - p0;
								Vector3 n = e2.Cross(e1);
								Float length2 = n.Length();
								if (length2 < 1e-8f)
								{
									continue;
								}
								n /= length2;

								Float dot = n.Dot(avgNormal);
								if (dot < minDot)
								{
									minDot = dot;
								}
							}
							bound.coneCutoff_ = minDot;
						}
					}
				}
			};

		/**
		* [EN]
		* Detects "boundary" vertices in a triangle mesh — vertices that
		* touch an edge used by exactly one triangle (an open/unwelded
		* edge). QuadricErrorMetrics::Simplify has no concept of boundaries
		* on its own: without locking, it happily collapses boundary edges
		* like any interior one, since a boundary vertex's quadric is only
		* built from its (one-sided) adjacent triangle planes and has no
		* term discouraging movement along the open edge. For a mesh made
		* of many small, non-closed parts (e.g. separately-exported prop
		* pieces that are meant to butt against each other, as opposed to
		* a single watertight mesh), this lets QEM shrink/warp every part's
		* silhouette at LOD1+ — the parts cave inward as their boundaries
		* get pulled toward the interior, which reads as the whole model
		* collapsing into flattened shards. Locking boundary vertices (see
		* QuadricErrorMetrics::Simplify's `locked` parameter) keeps their
		* position fixed — an edge with both endpoints on the boundary
		* becomes uncollapsible, preserving the silhouette, while interior
		* edges can still collapse onto a boundary vertex to reduce
		* triangle count.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 三角形メッシュの「境界」頂点を検出する — ちょうど 1 つの三角形からしか
		* 使われていないエッジ（開いた/溶接されていないエッジ）に接する頂点。
		* QuadricErrorMetrics::Simplify はそれ自体では境界という概念を持たない:
		* ロックしなければ、内部のエッジと同じように境界エッジも平気で崩壊させる。
		* 境界頂点の Quadric は片側の隣接三角形平面からしか構築されず、開いた
		* エッジに沿った移動を抑止する項が無いためである。多数の小さく閉じていない
		* パーツ（単一のウォータータイトメッシュではなく、互いに突き合わさる想定で
		* 別々にエクスポートされたプロップパーツなど）からなるメッシュでは、これにより
		* LOD1 以降で各パーツのシルエットが縮小/歪曲する — 境界が内側に引っ張られて
		* パーツが陥没し、モデル全体が潰れた破片の集まりに見える。境界頂点をロックする
		* （QuadricErrorMetrics::Simplify の `locked` 引数を参照）ことで位置を固定でき、
		* 両端点が境界にあるエッジは崩壊不可能になりシルエットが保たれる一方、内部の
		* エッジは境界頂点へ向けて崩壊でき三角形数の削減は引き続き行える。
		*/
		/// [EN] Returns std::unique_ptr<Bool[]> rather than DynamicArray<Bool>
		///      (= std::vector<bool>) on purpose: vector<bool> is bit-packed
		///      and has no .data(), so it can't produce the raw `const Bool*`
		///      QuadricErrorMetrics::Simplify's `locked` parameter needs.
		/// [JP] DynamicArray<Bool>（= std::vector<bool>）ではなく
		///      std::unique_ptr<Bool[]> を返すのは意図的: vector<bool> は
		///      ビットパック実装で .data() を持たないため、
		///      QuadricErrorMetrics::Simplify の `locked` 引数が要求する
		///      生の `const Bool*` を作れない。
		auto computeBoundaryLocks = [](const Uint32* indices, Size indexCount, Size vertexCount) -> std::unique_ptr<Bool[]>
			{
				std::unordered_map<Uint64, Uint32> edgeUseCount;
				Size triangleCount = indexCount / 3;
				for (Size triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
				{
					Uint32 triangleVertices[3] = { indices[triangleIndex * 3], indices[triangleIndex * 3 + 1], indices[triangleIndex * 3 + 2] };
					for (Int edgeIndex = 0; edgeIndex < 3; edgeIndex++)
					{
						Uint32 vertexA = triangleVertices[edgeIndex];
						Uint32 vertexB = triangleVertices[(edgeIndex + 1) % 3];
						if (vertexA > vertexB)
						{
							std::swap(vertexA, vertexB);
						}
						Uint64 key = (static_cast<Uint64>(vertexA) << 32) | vertexB;
						edgeUseCount[key]++;
					}
				}

				/// [EN] make_unique<Bool[]>(n) value-initialises every element to false.
				/// [JP] make_unique<Bool[]>(n) は全要素を false に値初期化する。
				std::unique_ptr<Bool[]> locked = std::make_unique<Bool[]>(vertexCount);
				for (const auto& [key, count] : edgeUseCount)
				{
					if (count == 1)
					{
						locked[static_cast<Uint32>(key >> 32)] = true;
						locked[static_cast<Uint32>(key & 0xFFFFFFFF)] = true;
					}
				}
				return locked;
			};

		/// [EN] Process each SubMesh independently: generate LOD 0, then
		///      repeatedly simplify with QEM to build LOD 1, 2, ... up to
		///      maxLodLevels. Each LOD gets its own set of meshlets (a Cluster).
		/// [JP] 各 SubMesh を独立に処理: LOD 0 を生成した後、QEM で繰り返し
		///      簡略化して LOD 1, 2, ... を maxLodLevels まで構築する。
		///      各 LOD は独自のメシュレットセット（Cluster）を持つ。
		for (SubMesh& sub : crister.subMeshes_)
		{
			sub.clusterOffset_ = static_cast<Uint32>(crister.clusters_.size());

			/// [EN] Extract this SubMesh's index slice from the original (pre-meshlet) indices.
			/// [JP] 元の（メシュレット化前の）インデックスからこの SubMesh のインデックススライスを抽出する。
			DynamicArray<Uint32> currentIndices(originalIndices.begin() + sub.indexOffset_, originalIndices.begin() + sub.indexOffset_ + sub.indexCount_);

			/**
			* [EN]
			* Build a local ↔ global vertex index mapping for this SubMesh.
			*
			* The indices in currentIndices are GLOBAL (into crister.vertices_).
			* QEM needs a DENSE local vertex array (0, 1, 2, ..., N-1), so we:
			*   1. Collect all unique global indices → localToGlobal
			*   2. Build the reverse map          → globalToLocal
			*   3. Extract local positions from crister.vertices_
			*   4. Remap the indices to local space
			*
			* After QEM simplifies, we map back to global to store the results.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* この SubMesh のローカル ↔ グローバル頂点インデックスマッピングを構築する。
			*
			* currentIndices 内のインデックスはグローバル（crister.vertices_ 内）。
			* QEM には密なローカル頂点配列 (0, 1, 2, ..., N-1) が必要なので:
			*   1. 全ユニークグローバルインデックスを収集 → localToGlobal
			*   2. 逆マップを構築                      → globalToLocal
			*   3. crister.vertices_ からローカル位置を抽出
			*   4. インデックスをローカル空間にリマップ
			*
			* QEM での簡略化後、結果を格納するためにグローバルに戻す。
			*/
			std::set<Uint32> usedSet(currentIndices.begin(), currentIndices.end());
			DynamicArray<Uint32> localToGlobal(usedSet.begin(), usedSet.end());
			std::unordered_map<Uint32, Uint32> globalToLocal;
			for (Uint32 index = 0; index < static_cast<Uint32>(localToGlobal.size()); index++)
			{
				globalToLocal[localToGlobal[index]] = index;
			}

			DynamicArray<Vector3> localPositions(localToGlobal.size());
			for (Size index = 0; index < localToGlobal.size(); index++)
			{
				localPositions[index] = crister.vertices_[localToGlobal[index]].position_;
			}

			DynamicArray<Uint32> localIndices(currentIndices.size());
			for (Size index = 0; index < currentIndices.size(); index++)
			{
				localIndices[index] = globalToLocal[currentIndices[index]];
			}

			/// [EN] LOD 0: Original detail. Build meshlets from unmodified indices.
			/// [JP] LOD 0: オリジナル詳細度。未変更のインデックスからメシュレットを構築。
			Uint32 meshletBegin = static_cast<Uint32>(crister.meshlets_.size());
			buildMeshletsFromIndices(currentIndices.data(), currentIndices.size());
			Uint32 meshletEnd = static_cast<Uint32>(crister.meshlets_.size());
			computeBounds(meshletBegin, meshletEnd);

			Cluster& lod0 = crister.clusters_.emplace_back();
			lod0.meshletOffset_ = meshletBegin;
			lod0.meshletCount_ = meshletEnd - meshletBegin;
			lod0.lodLevel_ = 0;
			lod0.lodError_ = 0.0f;

			/// [EN] State carried forward across LOD levels for iterative simplification.
			/// [JP] 反復的な簡略化のために LOD レベル間で持ち越す状態。
			DynamicArray<Vector3> lodPositions = localPositions;
			DynamicArray<Uint32> lodIndices = localIndices;

			/// [EN] lodVertexMap[localIndex] = global vertex index. Initially maps to the
			///      SubMesh's own vertices; updated each LOD as QEM creates new vertices.
			/// [JP] lodVertexMap[ローカルインデックス] = グローバル頂点インデックス。
			///      初期値は SubMesh 自身の頂点へのマップ。QEM が新頂点を作るたびに更新。
			DynamicArray<Uint32> lodVertexMap(localToGlobal.size());
			for (Uint32 index = 0; index < static_cast<Uint32>(localToGlobal.size()); index++)
			{
				lodVertexMap[index] = localToGlobal[index];
			}

			/// [EN] Cumulative error from LOD 0 to the current LOD. The AS uses this to decide
			///      whether this LOD is acceptable for the current screen-space pixel budget.
			///      It must be monotonically increasing so that LOD N+1 >= LOD N.
			/// [JP] LOD 0 から現在の LOD までの累積誤差。AS はこれを使って、この LOD が
			///      現在の画面空間ピクセル予算に対して許容可能かを判断する。
			///      LOD N+1 >= LOD N となるよう単調増加でなければならない。
			Float accumulatedError = 0.0f;

			/// [EN] LOD 1..N: Iteratively simplify with QEM.
			/// [JP] LOD 1..N: QEM で反復的に簡略化する。
			for (Uint32 lod = 1; lod < maxLodLevels; lod++)
			{
				Size currentTriangleCount = lodIndices.size() / 3;

				/// [EN] Target is 50% of the current triangle count. Stop if too few remain.
				/// [JP] 目標は現在の三角形数の 50%。残りが少なすぎたら停止。
				Size targetTriangleCount = currentTriangleCount / 2;
				if (targetTriangleCount < 4)
				{
					break;
				}

				/// [EN] Run QEM simplification on the current LOD's local mesh.
				///      Boundary vertices are recomputed every level (topology
				///      changes as edges collapse) and locked so open edges
				///      can't be pulled inward — see computeBoundaryLocks above.
				/// [JP] 現在の LOD のローカルメッシュに対して QEM 簡略化を実行する。
				///      境界頂点はレベルごとに再計算し（エッジ崩壊でトポロジーが
				///      変わるため）、開いたエッジが内側に引っ張られないようロックする
				///      — 上記 computeBoundaryLocks 参照。
				std::unique_ptr<Bool[]> boundaryLocks = computeBoundaryLocks(lodIndices.data(), lodIndices.size(), lodPositions.size());
				QuadricErrorMetrics qem;
				auto result = qem.Simplify(lodPositions.data(), lodPositions.size(), lodIndices.data(), lodIndices.size(), targetTriangleCount, boundaryLocks.get());

				/// [EN] If QEM couldn't reduce the mesh at all (e.g. all edges locked), stop.
				/// [JP] QEM がメッシュをまったく削減できなかった場合（全エッジがロックなど）、停止。
				if (result.indices.size() / 3 >= currentTriangleCount)
				{
					break;
				}

				/**
				* [EN]
				* Create new vertices for this LOD in crister.vertices_.
				*
				* QEM may have moved vertex positions to optimal locations, so we
				* can't reuse the original vertices. For each surviving vertex:
				*   1. Look up which original vertex it descended from (vertexMap).
				*   2. Copy that original vertex's attributes (normal, texcoord, etc.).
				*   3. Override the position with QEM's optimised position.
				*   4. Append as a new vertex at the end of crister.vertices_.
				*
				* This means each LOD level adds its own vertex copies. This is
				* intentional — the Mesh Shader will access the correct LOD's
				* vertices via the meshlet's vertexOffset_.
				*
				* ---------------------------------------------------------------------
				*
				* [JP]
				* この LOD の新しい頂点を crister.vertices_ に作成する。
				*
				* QEM は頂点位置を最適な場所に移動した可能性があるため、元の頂点は
				* 再利用できない。生き残った各頂点について:
				*   1. 元のどの頂点から派生したかを参照する (vertexMap)。
				*   2. その元の頂点の属性（法線、テクスチャ座標など）をコピーする。
				*   3. 位置を QEM の最適化された位置で上書きする。
				*   4. crister.vertices_ の末尾に新しい頂点として追加する。
				*
				* つまり各 LOD レベルが独自の頂点コピーを追加する。これは意図的で、
				* Mesh Shader がメシュレットの vertexOffset_ を通じて正しい LOD の
				* 頂点にアクセスする。
				*/
				Uint32 newVertexBase = static_cast<Uint32>(crister.vertices_.size());
				crister.vertices_.resize(newVertexBase + result.positions.size());
				crister.vertexMorphSource_.resize(newVertexBase + result.positions.size());

				DynamicArray<Uint32> newLodVertexMap(result.vertexMap.size());
				for (Size index = 0; index < result.positions.size(); index++)
				{
					Uint32 originalLocal = result.vertexMap[index];
					Uint32 globalIndex = lodVertexMap[originalLocal];

					/// [EN] Copy all attributes from the original vertex, then replace position.
					/// [JP] 元の頂点から全属性をコピーし、位置だけ置き換える。
					Vertex vertex = crister.vertices_[globalIndex];
					vertex.position_ = result.positions[index];
					crister.vertices_[newVertexBase + index] = vertex;

					/// [EN] Propagate forward rather than pointing at
					///      globalIndex directly - globalIndex may itself be
					///      a previous LOD's duplicated vertex, and
					///      vertexMorphSource_[globalIndex] already resolves
					///      to the TRUE original ancestor in that case (see
					///      vertexMorphSource_'s comment).
					/// [JP] globalIndex を直接指すのではなく前方へ伝播する —
					///      globalIndex 自体が前の LOD の複製頂点である
					///      場合があり、その場合 vertexMorphSource_
					///      [globalIndex] は既に真のオリジナル祖先へ解決
					///      済み(vertexMorphSource_ のコメント参照)。
					crister.vertexMorphSource_[newVertexBase + index] = crister.vertexMorphSource_[globalIndex];
					newLodVertexMap[index] = newVertexBase + static_cast<Uint32>(index);
				}

				/// [EN] Remap QEM's local indices to global indices (into crister.vertices_).
				/// [JP] QEM のローカルインデックスをグローバルインデックス（crister.vertices_ 内）にリマップする。
				DynamicArray<Uint32> globalLodIndices(result.indices.size());
				for (Size index = 0; index < result.indices.size(); index++)
				{
					globalLodIndices[index] = newVertexBase + result.indices[index];
				}

				/// [EN] Build meshlets for this LOD level and compute their bounds.
				/// [JP] この LOD レベルのメシュレットを構築し、バウンドを計算する。
				Uint32 lodMeshletBegin = static_cast<Uint32>(crister.meshlets_.size());
				buildMeshletsFromIndices(globalLodIndices.data(), globalLodIndices.size());
				Uint32 lodMeshletEnd = static_cast<Uint32>(crister.meshlets_.size());
				computeBounds(lodMeshletBegin, lodMeshletEnd);

				/**
				* [EN]
				* Compute this LOD level's geometric error and accumulate it.
				*
				* For each vertex that survived QEM, measure how far it moved
				* from its pre-simplification position. The maximum displacement
				* across all vertices is this level's error contribution.
				*
				* We ACCUMULATE (not replace) because LOD error must be monotonically
				* increasing for the AS LOD selection to work correctly. If LOD 1
				* introduced 0.5 units of error and LOD 2 introduces 0.3 more,
				* the total error at LOD 2 is 0.8, not 0.3.
				*
				* ---------------------------------------------------------------------
				*
				* [JP]
				* この LOD レベルの幾何学的誤差を計算し、累積する。
				*
				* QEM を生き残った各頂点について、簡略化前の位置からどれだけ移動したかを
				* 測定する。全頂点にわたる最大変位がこのレベルの誤差寄与となる。
				*
				* AS の LOD 選択が正しく機能するには LOD 誤差が単調増加でなければ
				* ならないため、置換ではなく累積する。LOD 1 で 0.5 単位の誤差が
				* 導入され、LOD 2 でさらに 0.3 導入された場合、LOD 2 での
				* 総誤差は 0.3 ではなく 0.8 である。
				*/
				Float maxLevelError = 0.0f;
				for (Size index = 0; index < result.positions.size(); index++)
				{
					Uint32 originalLocal = result.vertexMap[index];
					Vector3 difference = result.positions[index] - lodPositions[originalLocal];
					Float error = difference.Length();
					if (error > maxLevelError)
					{
						maxLevelError = error;
					}
				}
				accumulatedError += maxLevelError;

				/// [EN] Create a Cluster for this LOD level.
				/// [JP] この LOD レベルの Cluster を作成する。
				Cluster& lodCluster = crister.clusters_.emplace_back();
				lodCluster.meshletOffset_ = lodMeshletBegin;
				lodCluster.meshletCount_ = lodMeshletEnd - lodMeshletBegin;
				lodCluster.lodLevel_ = lod;
				lodCluster.lodError_ = accumulatedError;

				/// [EN] Carry state forward for the next LOD iteration.
				/// [JP] 次の LOD 反復のために状態を持ち越す。
				lodPositions = std::move(result.positions);
				lodIndices = std::move(result.indices);
				lodVertexMap = std::move(newLodVertexMap);
			}

			sub.clusterCount_ = static_cast<Uint32>(crister.clusters_.size()) - sub.clusterOffset_;
		}
	}

	/**
	* [EN]
	* Computes the global (world-space) transform for each node by walking
	* the scene graph tree depth-first.
	*
	* Each node's local transform is S · R · T (scale × rotation × translation).
	* The global transform is: localTransform × parentGlobalTransform.
	*
	* We use a stack to track the parent's global transform as we recurse
	* into children, pushing before descent and popping after return.
	*
	* Starting nodes come from the default scene's root node list, each
	* beginning with an identity parent transform.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シーングラフツリーを深さ優先で走査し、各ノードのグローバル
	* （ワールド空間）トランスフォームを計算する。
	*
	* 各ノードのローカルトランスフォームは S · R · T（スケール × 回転 × 平行移動）。
	* グローバルトランスフォームは: localTransform × parentGlobalTransform。
	*
	* スタックを使って再帰時に親のグローバルトランスフォームを追跡し、
	* 子に降りる前に push、戻った後に pop する。
	*
	* 開始ノードはデフォルトシーンのルートノードリストから取得し、
	* それぞれ単位行列の親トランスフォームから始める。
	*/
	void ModelLoader::CumulateTransforms(Crister& crister)
	{
		std::stack<Matrix> parentGlobalTransforms;
		std::function<void(Int)> traverse = [&](Int nodeIndex)
			{
				Node& node = crister.nodes_[nodeIndex];
				Matrix scale = Matrix::CreateScale(node.scale_.x, node.scale_.y, node.scale_.z);
				Matrix rotation = Matrix::CreateFromQuaternion(node.rotation_);
				Matrix translation = Matrix::CreateTranslation(node.translation_.x, node.translation_.y, node.translation_.z);

				/// [EN] Local transform = S · R · T, then multiplied by parent to get world-space.
				/// [JP] ローカルトランスフォーム = S · R · T、親と掛けてワールド空間に変換。
				Matrix localTransform = scale * rotation * translation;
				node.globalTransform_ = localTransform * parentGlobalTransforms.top();

				for (Int childIndex : node.children_)
				{
					parentGlobalTransforms.push(node.globalTransform_);
					traverse(childIndex);
					parentGlobalTransforms.pop();
				}
			};

		for (Int nodeIndex : crister.stages_[crister.defaultStage_].nodes_)
		{
			parentGlobalTransforms.push(Matrix::Identity);
			traverse(nodeIndex);
			parentGlobalTransforms.pop();
		}
	}

	void ModelLoader::ConvertAxisConvention(Crister& crister, const AxisConvention& axisConvention)
	{
		ResolvedAxisConvention resolved = ResolvedAxisConvention::Resolve(axisConvention);
		if (!resolved.valid_)
		{
			resolved = ResolvedAxisConvention::Resolve(AxisConvention{});
		}

		for (auto& node : crister.nodes_)
		{
			ConvertRotationByBasis(node.rotation_, resolved.basis_);
			ConvertPositionByBasis(node.translation_, resolved.basis_);
		}
		CumulateTransforms(crister);

		Float tangentSign = resolved.isMirror_ ? -1.0f : 1.0f;
		for (auto& vertex : crister.vertices_)
		{
			ConvertPositionByBasis(vertex.position_, resolved.basis_);
			ConvertPositionByBasis(vertex.normal_, resolved.basis_);
			ConvertPositionByBasis(vertex.tangent_, resolved.basis_);
			vertex.tangent_.w *= tangentSign;
		}

		for (auto& skin : crister.skins_)
		{
			for (Matrix& inverseBindMatrix : skin.inverseBindMatrices_)
			{
				ConvertMatrixByBasis(inverseBindMatrix, resolved.basis_);
			}
		}

		if (resolved.flipWinding_)
		{
			FlipTriangleWinding(crister);
		}
	}

	void ModelLoader::FetchLights(const tinygltf::Model& model, Crister& crister)
	{
		for (const Node& node : crister.nodes_)
		{
			if (node.light_ < 0 || node.light_ >= static_cast<Int>(model.lights.size()))
			{
				continue;
			}

			const tinygltf::Light& gltfLight = model.lights[node.light_];

			/// [EN] This engine's directional light is authored per-scene (see
			///      DirectionalLight component), not embedded per-model, so
			///      directional KHR lights are intentionally skipped here.
			/// [JP] このエンジンのディレクショナルライトはモデル単位ではなく
			///      シーン単位で設定するため（DirectionalLight コンポーネント参照）、
			///      ディレクショナルの KHR ライトはここで意図的にスキップする。
			if (gltfLight.type == "directional")
			{
				continue;
			}

			PunctualLight light{};
			light.type_ = (gltfLight.type == "spot") ? PunctualLight::Type::Spot : PunctualLight::Type::Point;

			/// [EN] Position = world translation of the referencing node.
			///      Direction: glTF punctual lights shine down local -Z; the
			///      node's world -Z axis is the third row of its (row-major,
			///      row-vector) global transform, negated.
			/// [JP] 位置 = 参照ノードのワールド平行移動。向き: glTF のパンクチュアル
			///      ライトはローカル -Z 方向に照射する。ノードのワールド -Z 軸は
			///      グローバルトランスフォーム（row-major, 行ベクトル規約）の
			///      3行目を反転したもの。
			const Matrix& worldTransform = node.globalTransform_;
			light.position_ = worldTransform.Translation();

			Vector3 worldForward(worldTransform._31, worldTransform._32, worldTransform._33);
			worldForward.Normalize();
			light.direction_ = -worldForward;

			if (gltfLight.color.size() == 3)
			{
				light.colorRGB_[0] = static_cast<Float>(gltfLight.color[0]);
				light.colorRGB_[1] = static_cast<Float>(gltfLight.color[1]);
				light.colorRGB_[2] = static_cast<Float>(gltfLight.color[2]);
			}

			light.intensity_ = static_cast<Float>(gltfLight.intensity);

			/// [EN] glTF: range == 0 (or unset) means "unbounded". This engine's
			///      attenuation shader (AttenuateDistance, Light.hlsli) divides
			///      by range, so an unbounded light needs a finite fallback.
			/// [JP] glTF: range == 0（未設定）は「無制限」を意味する。このエンジンの
			///      減衰シェーダ(AttenuateDistance, Light.hlsli)は range で除算する
			///      ため、無制限ライトには有限のフォールバック値が必要。
			constexpr Float unboundedRangeFallback = 10.0f;
			Float range = static_cast<Float>(gltfLight.range);
			light.range_ = (range > 0.0f) ? range : unboundedRangeFallback;

			if (light.type_ == PunctualLight::Type::Spot)
			{
				light.innerConeAngle_ = static_cast<Float>(gltfLight.spot.innerConeAngle);
				light.outerConeAngle_ = static_cast<Float>(gltfLight.spot.outerConeAngle);
			}

			crister.lights_.push_back(light);
		}
	}

	/**
	* [EN]
	* Transforms a position/direction Vector3 by a change-of-basis matrix.
	* Generalizes the old fixed X-negation to an arbitrary signed-permutation basis.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 位置/方向を表す Vector3 を基底変換行列で変換する。
	* 旧来の固定X反転を、任意の符号付き置換基底へ一般化したもの。
	*/
	void ModelLoader::ConvertPositionByBasis(Vector3& vector, const Matrix& basis)
	{
		vector = Vector3::Transform(vector, basis);
	}

	/**
	* [EN]
	* Transforms the .xyz of a Vector4 (e.g. tangent) by basis. .w
	* (handedness sign) is intentionally left untouched here - the caller
	* is responsible for flipping it when the basis is a mirror.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Vector4（タンジェント等）の .xyz を basis で変換する。.w（利き手符号）
	* はここでは意図的に触らない — 基底がミラーの場合の符号反転は
	* 呼び出し側の責任とする。
	*/
	void ModelLoader::ConvertPositionByBasis(Vector4& vector, const Matrix& basis)
	{
		Vector3 xyz = Vector3::Transform(Vector3(vector.x, vector.y, vector.z), basis);
		vector.x = xyz.x;
		vector.y = xyz.y;
		vector.z = xyz.z;
	}

	/**
	* [EN]
	* Conjugates a rotation quaternion by an orthogonal change-of-basis:
	* R' = basis^T * R * basis. Generalizes the old fixed "negate y/z"
	* hack, which was this same conjugation specialized to basis = diag(-1,1,1).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 回転クォータニオンを直交基底変換で共役変換する: R' = basis^T * R * basis。
	* 旧来の固定「y,z符号反転」は、この共役変換を basis = diag(-1,1,1) に
	* 特殊化したものに等しい。
	*/
	void ModelLoader::ConvertRotationByBasis(Quaternion& quaternion, const Matrix& basis)
	{
		Matrix rotation = Matrix::CreateFromQuaternion(quaternion);
		Matrix converted = basis.Transpose() * rotation * basis;
		quaternion = Quaternion::CreateFromRotationMatrix(converted);
	}

	/**
	* [EN]
	* Conjugates a full 4x4 transform (e.g. an inverse bind matrix) by
	* basis: matrix' = basis^T * matrix * basis. Since basis's own
	* translation row is identity, this correctly transforms both the
	* linear part and the translation in one step.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 4x4変換（逆バインド行列等）を basis で共役変換する:
	* matrix' = basis^T * matrix * basis。basis 自身の平行移動行は単位
	* なので、線形部分と平行移動を1回で正しく変換できる。
	*/
	void ModelLoader::ConvertMatrixByBasis(Matrix& matrix, const Matrix& basis)
	{
		matrix = basis.Transpose() * matrix * basis;
	}

	/**
	* [EN]
	* Reverses triangle winding by swapping the last two corners of every
	* triangle in crister.vertexIndices_ (the flat, pre-meshlet global
	* index list populated by FetchMeshes). Only invoked when the
	* resolved axis conversion explicitly requires a winding flip.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* crister.vertexIndices_（FetchMeshes が生成する、メシュレット化前の
	* フラットなグローバルインデックス列）の全三角形について後ろ2頂点を
	* 入れ替え、巻き順を反転する。解決済みの軸変換が巻き順反転を
	* 明示的に要求する時のみ呼ばれる。
	*/
	void ModelLoader::FlipTriangleWinding(Crister& crister)
	{
		Size triangleCount = crister.vertexIndices_.size() / 3;
		for (Size triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
		{
			std::swap(crister.vertexIndices_[triangleIndex * 3 + 1], crister.vertexIndices_[triangleIndex * 3 + 2]);
		}
	}
}
