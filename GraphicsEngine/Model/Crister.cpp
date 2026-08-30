#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <GraphicsEngine/Model/BC7CompressShader.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
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
	Crister::~Crister()
	{
		auto found = std::ranges::find(streamingRegistry_, this);
		if (found != streamingRegistry_.end())
		{
			streamingRegistry_.erase(found);
		}

		if (!bindlessHeap_)
		{
			return;
		}

		/// [EN] Release every resident page (pinned included — the model itself
		///      is going away) and give the descriptor indices back to the heap.
		///      Every GPU resource is handed to the deferred-reclaim ring rather
		///      than dying with this object: ModelLoader::Clear destroys the
		///      Crister synchronously (ModelResource::Unload, which the editor's
		///      model-conversion "適用" runs mid-session), and the frames still in
		///      flight are drawing from exactly these buffers and textures.
		/// [JP] 常駐ページをすべて解放し（モデル自体が消えるためピン留め込み）、
		///      ディスクリプタインデックスをヒープへ返す。GPU リソースはこの
		///      オブジェクトと一緒に死なせず、遅延回収リングへ渡す:
		///      ModelLoader::Clear は Crister を同期的に破棄し
		///      (ModelResource::Unload — エディタのモデル変換「適用」が実行中に
		///      呼ぶ)、インフライトのフレームはまさにこれらのバッファと
		///      テクスチャで描画している最中だから。
		for (StreamingGeometry& page : streamingGeometry_)
		{
			if (!page.resident_)
			{
				continue;
			}
			bindlessHeap_->FreeIndex(page.meshletBufferIndex_);
			bindlessHeap_->FreeIndex(page.meshletBoundBufferIndex_);
			bindlessHeap_->FreeIndex(page.vertexIndicesBufferIndex_);
			bindlessHeap_->FreeIndex(page.primitiveIndicesBufferIndex_);
			bindlessHeap_->DeferRelease(page.meshletResource_);
			bindlessHeap_->DeferRelease(page.meshletBoundResource_);
			bindlessHeap_->DeferRelease(page.vertexIndicesResource_);
			bindlessHeap_->DeferRelease(page.primitiveIndicesResource_);
			if (page.ownsVertices_)
			{
				bindlessHeap_->FreeIndex(page.vertexBufferIndex_);
				bindlessHeap_->DeferRelease(page.vertexResource_);
			}
			totalResidentGeometryBytes_ -= page.sizeBytes_;
			page.resident_ = false;
		}

		if (poolResident_)
		{
			bindlessHeap_->FreeIndex(poolBufferIndex_);
			bindlessHeap_->DeferRelease(poolResource_);
			totalResidentGeometryBytes_ -= poolSizeBytes_;
			poolResident_ = false;
		}

		for (StreamingTexture& streamingTexture : streamingTextures_)
		{
			if (streamingTexture.pinnedMip_.resource_)
			{
				bindlessHeap_->FreeIndex(streamingTexture.pinnedMip_.bindlessIndex_);
				bindlessHeap_->DeferRelease(streamingTexture.pinnedMip_.resource_);
				totalResidentTextureBytes_ -= streamingTexture.pinnedMip_.sizeBytes_;
			}
			if (streamingTexture.currentMip_.resource_)
			{
				bindlessHeap_->FreeIndex(streamingTexture.currentMip_.bindlessIndex_);
				bindlessHeap_->DeferRelease(streamingTexture.currentMip_.resource_);
				totalResidentTextureBytes_ -= streamingTexture.currentMip_.sizeBytes_;
			}
		}
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
	void Crister::BakeMesh()
	{
		/// [EN] Compute the dequantisation AABBs, then quantise every vertex into
		///      the 16-byte GPU format (and skinning attributes, if skinned).
		///      Extents are clamped away from zero so flat axes still
		///      dequantise to the shared min value. Must run after
		///      BuildMeshlets, since QEM appends fresh vertex copies per LOD.
		/// [JP] 逆量子化 AABB を計算し、全頂点を 16 バイトの GPU フォーマット
		///      （とスキンがあればスキニング属性）へ量子化する。extent はゼロ
		///      から離してクランプし、平坦な軸でも共通の min 値へ正しく逆量子化
		///      されるようにする。QEM が LOD ごとに新頂点を追加するため、
		///      BuildMeshlets の後に実行すること。
		{
			Vector3 positionMax = Vector3(0, 0, 0);
			Vector2 texcoordMax = Vector2(0, 0);
			positionMin_ = Vector3(0, 0, 0);
			texcoordMin_ = Vector2(0, 0);

			if (!vertices_.empty())
			{
				positionMin_ = vertices_[0].position_;
				positionMax = vertices_[0].position_;
				texcoordMin_ = vertices_[0].texcoord_;
				texcoordMax = vertices_[0].texcoord_;
				for (const Vertex& vertex : vertices_)
				{
					positionMin_ = Vector3::Min(positionMin_, vertex.position_);
					positionMax = Vector3::Max(positionMax, vertex.position_);
					texcoordMin_ = Vector2::Min(texcoordMin_, vertex.texcoord_);
					texcoordMax = Vector2::Max(texcoordMax, vertex.texcoord_);
				}
			}

			positionExtent_ = Vector3::Max(positionMax - positionMin_, Vector3(1e-6f, 1e-6f, 1e-6f));
			texcoordExtent_ = Vector2::Max(texcoordMax - texcoordMin_, Vector2(1e-6f, 1e-6f));
		}

		compressedVertices_.resize(vertices_.size());
		for (Size vertexIndex = 0; vertexIndex < vertices_.size(); vertexIndex++)
		{
			compressedVertices_[vertexIndex] = EncodeVertex(vertices_[vertexIndex], positionMin_, positionExtent_, texcoordMin_, texcoordExtent_);
		}

		if (!skins_.empty())
		{
			compressedSkinVertices_.resize(vertices_.size());
			for (Size vertexIndex = 0; vertexIndex < vertices_.size(); vertexIndex++)
			{
				const Vertex& vertex = vertices_[vertexIndex];
				CompressedSkinVertex& skin = compressedSkinVertices_[vertexIndex];
				skin.jointsXY_ = (vertex.joints_.x & 0xFFFF) | ((vertex.joints_.y & 0xFFFF) << 16);
				skin.jointsZW_ = (vertex.joints_.z & 0xFFFF) | ((vertex.joints_.w & 0xFFFF) << 16);

				skin.weights_ =
					QuantizeUnorm8(vertex.weights_.x) |
					(QuantizeUnorm8(vertex.weights_.y) << 8) |
					(QuantizeUnorm8(vertex.weights_.z) << 16) |
					(QuantizeUnorm8(vertex.weights_.w) << 24);
			}
		}

		vertices_.clear();
		vertices_.shrink_to_fit();
	}

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
	void Crister::BakeBitmap(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BC7CompressShader& bc7Shader)
	{
		/// [JP] RGBA8 画像を整数倍率のボックスフィルタで縮小する（D3D12 の 2D
		///      テクスチャ最大寸法 16384 を超える場合のみ呼ばれる）。
		auto downsampleRgbaToFit = [](DynamicArray<Uchar>& rgba, Int& width, Int& height, Int maxDimension)
		{
			Int largest = width > height ? width : height;
			Int factor = (largest + maxDimension - 1) / maxDimension;
			if (factor <= 1)
			{
				return;
			}

			Int newWidth = width / factor;
			Int newHeight = height / factor;
			DynamicArray<Uchar> shrunk(static_cast<Size>(newWidth) * newHeight * 4);

			for (Int y = 0; y < newHeight; y++)
			{
				for (Int x = 0; x < newWidth; x++)
				{
					Uint32 sum[4] = { 0, 0, 0, 0 };
					for (Int sampleY = 0; sampleY < factor; sampleY++)
					{
						for (Int sampleX = 0; sampleX < factor; sampleX++)
						{
							Size source = (static_cast<Size>(y) * factor + sampleY) * width + (static_cast<Size>(x) * factor + sampleX);
							for (Int component = 0; component < 4; component++)
							{
								sum[component] += rgba[source * 4 + component];
							}
						}
					}

					Size destination = static_cast<Size>(y) * newWidth + x;
					Uint32 sampleCount = static_cast<Uint32>(factor) * factor;
					for (Int component = 0; component < 4; component++)
					{
						shrunk[destination * 4 + component] = static_cast<Uchar>(sum[component] / sampleCount);
					}
				}
			}

			rgba = std::move(shrunk);
			width = newWidth;
			height = newHeight;
		};

		/// [JP] 透明(α=0)テクセルのRGBを、隣接する不透明テクセルの色で反復的に埋める
		///      （dilation / edge padding）。αは変えない。透明部のRGBは黒のことが多く、
		///      これを埋めずに BC7 圧縮・ミップ生成すると、その黒がカットアウトの縁へ
		///      滲んで黒フチになる。色だけ隣から埋めておけば防げる。BC7 圧縮後は
		///      テクセル単位で触れなくなるため、圧縮前のこの時点でやる必要がある。
		auto dilateTransparentColor = [](Uchar* pixels, Int width, Int height)
		{
			const Size w = static_cast<Size>(width);
			const Size h = static_cast<Size>(height);
			const Size rowPitch = w * 4;

			auto Texel = [&](Size x, Size y) -> Uchar*
			{
				return pixels + y * rowPitch + x * 4;
			};

			DynamicArray<Uint8> known(w * h);
			for (Size y = 0; y < h; y++)
			{
				for (Size x = 0; x < w; x++)
				{
					known[y * w + x] = Texel(x, y)[3] > 0 ? 1 : 0;
				}
			}

			Bool changed = true;
			Uint pass = 0;
			while (changed && pass < 64)
			{
				changed = false;
				pass++;

				/// [JP] このパスで埋めた分はパス終了後に known 化（同一パス内で滲ませない）。
				DynamicArray<Size> filled;
				for (Size y = 0; y < h; y++)
				{
					for (Size x = 0; x < w; x++)
					{
						if (known[y * w + x])
						{
							continue;
						}

						Uint accumR = 0;
						Uint accumG = 0;
						Uint accumB = 0;
						Uint count = 0;
						for (Int dy = -1; dy <= 1; dy++)
						{
							for (Int dx = -1; dx <= 1; dx++)
							{
								if (dx == 0 && dy == 0)
								{
									continue;
								}
								const Int nx = static_cast<Int>(x) + dx;
								const Int ny = static_cast<Int>(y) + dy;
								if (nx < 0 || ny < 0 || nx >= width || ny >= height)
								{
									continue;
								}
								if (!known[static_cast<Size>(ny) * w + static_cast<Size>(nx)])
								{
									continue;
								}
								const Uchar* neighbor = Texel(static_cast<Size>(nx), static_cast<Size>(ny));
								accumR += neighbor[0];
								accumG += neighbor[1];
								accumB += neighbor[2];
								count++;
							}
						}

						if (count > 0)
						{
							Uchar* destination = Texel(x, y);
							destination[0] = static_cast<Uchar>(accumR / count);
							destination[1] = static_cast<Uchar>(accumG / count);
							destination[2] = static_cast<Uchar>(accumB / count);
							filled.push_back(y * w + x);
							changed = true;
						}
					}
				}

				for (const Size index : filled)
				{
					known[index] = 1;
				}
			}
		};

		/// [JP] 1ミップぶんをGPUでBC7圧縮してoutputBlocksへ読み戻す。完全同期:
		///      専用の一発コマンドリストとフェンスを作り、GPUが終わるまで
		///      呼び出しスレッドをブロックする。ルートシグネチャ/PSOは
		///      bc7Shader(Graphics所有、起動時に一度だけ構築)のものを使う。
		auto compressBC7 = [device, cmdQueue, &bc7Shader](const DirectX::Image& image, DynamicArray<Uchar>& outputBlocks)
		{
			Uint32 width = static_cast<Uint32>(image.width);
			Uint32 height = static_cast<Uint32>(image.height);
			Uint32 blockCountX = (width + 3) / 4;
			Uint32 blockCountY = (height + 3) / 4;
			Uint32 blockCount = blockCountX * blockCountY;

			DynamicArray<Uint32> packedPixels(static_cast<Size>(width) * height);
			for (Size y = 0; y < height; y++)
			{
				const Uchar* row = image.pixels + y * image.rowPitch;
				for (Size x = 0; x < width; x++)
				{
					const Uchar* texel = row + x * 4;
					packedPixels[y * width + x] = texel[0] | (texel[1] << 8) | (texel[2] << 16) | (texel[3] << 24);
				}
			}

			HRESULT hr{ S_OK };

			D3D12_HEAP_PROPERTIES uploadHeapProperties{};
			uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

			D3D12_RESOURCE_DESC inputDesc{};
			inputDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			inputDesc.Width = static_cast<UINT64>(packedPixels.size()) * sizeof(Uint32);
			inputDesc.Height = 1;
			inputDesc.DepthOrArraySize = 1;
			inputDesc.MipLevels = 1;
			inputDesc.Format = DXGI_FORMAT_UNKNOWN;
			inputDesc.SampleDesc.Count = 1;
			inputDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			Microsoft::WRL::ComPtr<ID3D12Resource> inputBuffer;
			hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &inputDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(inputBuffer.GetAddressOf()));
			SC_HR_CHECK(hr, "入力バッファの生成に失敗しました");
#ifdef _DEBUG
			inputBuffer->SetName(L"Crister_CompressInput");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(inputBuffer.Get());
#endif

			void* mappedInput = nullptr;
			D3D12_RANGE noRead{ 0, 0 };
			hr = inputBuffer->Map(0, &noRead, &mappedInput);
			SC_HR_CHECK(hr, "入力バッファのMapに失敗しました");
			memcpy(mappedInput, packedPixels.data(), packedPixels.size() * sizeof(Uint32));
			inputBuffer->Unmap(0, nullptr);

			D3D12_HEAP_PROPERTIES defaultHeapProperties{};
			defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC outputDesc{};
			outputDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			outputDesc.Width = static_cast<UINT64>(blockCount) * 16;
			outputDesc.Height = 1;
			outputDesc.DepthOrArraySize = 1;
			outputDesc.MipLevels = 1;
			outputDesc.Format = DXGI_FORMAT_UNKNOWN;
			outputDesc.SampleDesc.Count = 1;
			outputDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			outputDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			Microsoft::WRL::ComPtr<ID3D12Resource> outputBuffer;
			hr = device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &outputDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(outputBuffer.GetAddressOf()));
			SC_HR_CHECK(hr, "出力バッファの生成に失敗しました");
#ifdef _DEBUG
			outputBuffer->SetName(L"Crister_CompressOutput");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(outputBuffer.Get());
#endif

			D3D12_HEAP_PROPERTIES readbackHeapProperties{};
			readbackHeapProperties.Type = D3D12_HEAP_TYPE_READBACK;

			D3D12_RESOURCE_DESC readbackDesc = outputDesc;
			readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
			hr = device->CreateCommittedResource(&readbackHeapProperties, D3D12_HEAP_FLAG_NONE, &readbackDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(readbackBuffer.GetAddressOf()));
			SC_HR_CHECK(hr, "リードバックバッファの生成に失敗しました");
#ifdef _DEBUG
			readbackBuffer->SetName(L"Crister_CompressReadback");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(readbackBuffer.Get());
#endif

			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
			hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAllocator.GetAddressOf()));
			SC_HR_CHECK(hr, "コマンドアロケーターの生成に失敗しました");

			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
			hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf()));
			SC_HR_CHECK(hr, "コマンドリストの生成に失敗しました");

			cmdList->SetComputeRootSignature(bc7Shader.GetRootSignature());
			cmdList->SetPipelineState(bc7Shader.GetPipelineState());

			Uint32 constants[4] = { width, height, blockCountX, blockCountY };
			cmdList->SetComputeRoot32BitConstants(0, 4, constants, 0);
			cmdList->SetComputeRootShaderResourceView(1, inputBuffer->GetGPUVirtualAddress());
			cmdList->SetComputeRootUnorderedAccessView(2, outputBuffer->GetGPUVirtualAddress());

			cmdList->Dispatch((blockCountX + 7) / 8, (blockCountY + 7) / 8, 1);

			D3D12_RESOURCE_BARRIER toCopySource{};
			toCopySource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			toCopySource.Transition.pResource = outputBuffer.Get();
			toCopySource.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			toCopySource.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
			toCopySource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			cmdList->ResourceBarrier(1, &toCopySource);

			cmdList->CopyResource(readbackBuffer.Get(), outputBuffer.Get());

			hr = cmdList->Close();
			SC_HR_CHECK(hr, "コマンドリストのCloseに失敗しました");

			ID3D12CommandList* lists[] = { cmdList.Get() };
			Microsoft::WRL::ComPtr<ID3D12Fence> fence;
			hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
			SC_HR_CHECK(hr, "フェンスの生成に失敗しました");
			{
				/// [EN] Lock only around the submission itself (queue-touching,
				///      fast) - not the wait below, which just blocks this
				///      thread on a fence event and never touches the queue.
				/// [JP] 提出そのもの(キューに触れる、高速な部分)だけをロックする
				///      - 下の待機はこのスレッドをフェンスイベントで止めるだけで
				///      キューには一切触れない。
				auto queueLock = cmdQueue->AcquireLock();
				cmdQueue->GetCommandQueue()->ExecuteCommandLists(1, lists);
				hr = cmdQueue->GetCommandQueue()->Signal(fence.Get(), 1);
				SC_HR_CHECK(hr, "コマンドキューのSignalに失敗しました");
			}
			if (fence->GetCompletedValue() < 1)
			{
				HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				fence->SetEventOnCompletion(1, event);
				WaitForSingleObject(event, INFINITE);
				CloseHandle(event);
			}

			outputBlocks.resize(static_cast<Size>(blockCount) * 16);
			void* mappedReadback = nullptr;
			D3D12_RANGE readRange{ 0, outputBlocks.size() };
			hr = readbackBuffer->Map(0, &readRange, &mappedReadback);
			SC_HR_CHECK(hr, "リードバックバッファのMapに失敗しました");
			memcpy(outputBlocks.data(), mappedReadback, outputBlocks.size());
			D3D12_RANGE noWrite{ 0, 0 };
			readbackBuffer->Unmap(0, &noWrite);
		};

		for (Bitmap& texture : bitmaps_)
		{
			if (texture.width_ <= 0 || texture.height_ <= 0 || texture.cacheData_.empty() || texture.bits_ != 8)
			{
				continue;
			}

			/// [EN] Always produce a mutable RGBA8 buffer (expanding when the decoded
			///      image has fewer than 4 components) so it can be dilated in place.
			/// [JP] dilation で書き換えるため、常に可変の RGBA8 バッファを作る
			///      （4 コンポーネント未満なら展開、4 ならコピー）。
			Size pixelCount = static_cast<Size>(texture.width_) * static_cast<Size>(texture.height_);
			DynamicArray<Uchar> rgba(pixelCount * 4);
			if (texture.component_ == 4)
			{
				memcpy(rgba.data(), texture.cacheData_.data(), pixelCount * 4);
			}
			else
			{
				for (Size pixel = 0; pixel < pixelCount; pixel++)
				{
					for (Int component = 0; component < 4; component++)
					{
						if (component < texture.component_)
						{
							rgba[pixel * 4 + component] = texture.cacheData_[pixel * texture.component_ + component];
						}
						else
						{
							rgba[pixel * 4 + component] = component == 3 ? 255 : 0;
						}
					}
				}
			}

			/// [JP] D3D12 の 2D テクスチャ最大寸法(16384)を超える画像は作成自体が
			///      失敗するため、先に CPU 側でボックス縮小して収める。
			Int textureWidth = texture.width_;
			Int textureHeight = texture.height_;
			if (textureWidth > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || textureHeight > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
			{
				SC_LOG_WARNING("テクスチャ {} が D3D12 の最大寸法(16384)を超えています({}x{})。上限内に縮小して読み込みます。", texture.name_, textureWidth, textureHeight);
				downsampleRgbaToFit(rgba, textureWidth, textureHeight, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
			}

			/// [EN] Fill transparent-texel colors before mip generation to kill cutout halos.
			/// [JP] ミップ生成前に透明テクセルの色を埋め、カットアウトの黒フチを防ぐ。
			dilateTransparentColor(rgba.data(), textureWidth, textureHeight);

			DirectX::Image sourceImage{};
			sourceImage.width = static_cast<Size>(textureWidth);
			sourceImage.height = static_cast<Size>(textureHeight);
			sourceImage.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sourceImage.rowPitch = static_cast<Size>(textureWidth) * 4;
			sourceImage.slicePitch = sourceImage.rowPitch * static_cast<Size>(textureHeight);
			sourceImage.pixels = rgba.data();

			HRESULT hr{ S_OK };
			DirectX::ScratchImage mipChain;
			hr = DirectX::GenerateMipMaps(sourceImage, DirectX::TEX_FILTER_BOX, 0, mipChain);
			if (FAILED(hr))
			{
				SC_LOG_WARNING("テクスチャ {} のミップ生成に失敗しました(HRESULT=0x{:08X})。このテクスチャ抜きで続行します。", texture.name_, static_cast<Uint32>(hr));
				continue;
			}

			/// [JP] CPU版DirectXTexのBC7圧縮(デフォルト品質はもちろんQUICKモードでも)は
			///      非常に遅く、TEX_COMPRESS_PARALLELも同梱DirectXTex.libがOpenMP付き
			///      ビルドでない限り効かない。ミップごとにGPUでBC7圧縮する。
			texture.width_ = textureWidth;
			texture.height_ = textureHeight;
			texture.mipCount_ = static_cast<Int>(mipChain.GetMetadata().mipLevels);
			texture.cacheData_.clear();
			for (Size mip = 0; mip < mipChain.GetImageCount(); mip++)
			{
				DynamicArray<Uchar> mipBlocks;
				compressBC7(*mipChain.GetImage(mip, 0, 0), mipBlocks);
				texture.cacheData_.insert(texture.cacheData_.end(), mipBlocks.begin(), mipBlocks.end());
			}
		}
	}

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
	Bool Crister::BakeCollision(MeshCollisionDetail detail, DynamicArray<Vector3>& outPositions, DynamicArray<Uint32>& outIndices)const
	{
		outPositions.clear();
		outIndices.clear();

		if (compressedVertices_.empty() || subMeshes_.empty())
		{
			return false;
		}

		/// [EN] Maps a global (pre-dedup) vertex index to its position in
		///      outPositions, so triangles sharing a vertex reuse the same
		///      compact index instead of duplicating the position.
		/// [JP] グローバル（重複排除前）の頂点インデックスを outPositions 内の
		///      位置へ対応付ける。頂点を共有する三角形が同じコンパクト
		///      インデックスを再利用し、位置が重複しないようにする。
		std::unordered_map<Uint32, Uint32> remap;

		for (const SubMesh& subMesh : subMeshes_)
		{
			if (subMesh.clusterCount_ == 0)
			{
				continue;
			}

			/// [EN] Exact takes the SubMesh's first (LOD 0, most detailed)
			///      cluster; Proxy takes its last (coarsest) cluster — same
			///      range the RT proxy geometry already uses.
			/// [JP] Exact は SubMesh の最初のクラスタ（LOD 0、最も詳細）、
			///      Proxy は最後のクラスタ（最も粗い）を取る — RT プロキシ
			///      ジオメトリが既に使っているのと同じ範囲。
			Uint32 clusterIndex = detail == MeshCollisionDetail::Exact ? subMesh.clusterOffset_ : subMesh.clusterOffset_ + subMesh.clusterCount_ - 1;

			if (clusterIndex >= clusters_.size())
			{
				continue;
			}

			/// [EN] Walk every meshlet in the chosen cluster, then every
			///      triangle corner in each meshlet, resolving each corner
			///      through vertexIndices_/primitiveIndices_ to a global
			///      vertex index.
			/// [JP] 選んだクラスタ内の全メシュレットを走査し、各メシュレット内の
			///      全三角形の各頂点を、vertexIndices_/primitiveIndices_ 経由で
			///      グローバル頂点インデックスへ解決する。
			const Cluster& cluster = clusters_[clusterIndex];
			for (Uint32 meshletIndex = cluster.meshletOffset_; meshletIndex < cluster.meshletOffset_ + cluster.meshletCount_; meshletIndex++)
			{
				const Meshlet& meshlet = meshlets_[meshletIndex];
				for (Uint32 triangleIndex = 0; triangleIndex < meshlet.triangleCount_; triangleIndex++)
				{
					Uint32 byteOffset = meshlet.triangleOffset_ + triangleIndex * 3;
					for (Int corner = 0; corner < 3; corner++)
					{
						Uint32 globalIndex = vertexIndices_[meshlet.vertexOffset_ + primitiveIndices_[byteOffset + corner]];
						auto found = remap.find(globalIndex);
						if (found == remap.end())
						{
							/// [EN] First time this global vertex is seen: decode its
							///      position and append it, remembering the compact
							///      index for later corners that share it.
							/// [JP] このグローバル頂点を初めて見た場合: 位置を
							///      デコードして追加し、後で同じ頂点を共有する
							///      角のためにコンパクトインデックスを記憶する。
							Uint32 compactIndex = static_cast<Uint32>(outPositions.size());
							remap[globalIndex] = compactIndex;
							outPositions.push_back(DecodePosition(compressedVertices_[globalIndex]));
							outIndices.push_back(compactIndex);
						}
						else
						{
							/// [EN] Already emitted: reuse its compact index instead
							///      of pushing a duplicate position.
							/// [JP] 既に出力済み: 位置を重複追加せず、そのコンパクト
							///      インデックスを再利用する。
							outIndices.push_back(found->second);
						}
					}
				}
			}
		}

		return outIndices.size() >= 3;
	}

	/**
	* [EN]
	* Reads this Crister's LOD 0 geometry into a CPU-side full-vertex/index
	* pair addressed by the same global vertex index space
	* compressedVertices_/vertexIndices_ already use. Not part of the
	* bake/cache pipeline — called live off the already-loaded Crister.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この Crister の LOD 0 ジオメトリを、compressedVertices_/vertexIndices_
	* が既に使っているのと同じグローバル頂点インデックス空間で CPU 側の
	* フル頂点/インデックス対へ読み出す。ベイク/キャッシュパイプラインの
	* 一部ではなく、ロード済みの Crister に対してその都度呼び出す。
	*/
	Bool Crister::SoftbodyFinestVertices(DynamicArray<Vertex>& outVertices, DynamicArray<Uint32>& outIndices)const
	{
		outVertices.clear();
		outIndices.clear();

		if (compressedVertices_.empty() || subMeshes_.empty())
		{
			return false;
		}

		/// [EN] Unlike BakeCollision, no remap: outVertices is a 1:1 copy of
		///      compressedVertices_ (decoded), so a global vertex index can
		///      be used directly as an index into it.
		/// [JP] BakeCollision と違いリマップは行わない: outVertices は
		///      compressedVertices_ の 1:1 コピー（デコード済み）なので、
		///      グローバル頂点インデックスをそのままインデックスとして使える。
		outVertices.reserve(compressedVertices_.size());
		for (const CompressedVertex& compressed : compressedVertices_)
		{
			outVertices.push_back(DecodeVertex(compressed));
		}

		for (const SubMesh& subMesh : subMeshes_)
		{
			if (subMesh.clusterCount_ == 0)
			{
				continue;
			}

			/// [EN] LOD 0 is always the SubMesh's first cluster.
			/// [JP] LOD 0 は常に SubMesh の最初のクラスタ。
			Uint32 clusterIndex = subMesh.clusterOffset_;
			if (clusterIndex >= clusters_.size())
			{
				continue;
			}

			const Cluster& cluster = clusters_[clusterIndex];
			for (Uint32 meshletIndex = cluster.meshletOffset_; meshletIndex < cluster.meshletOffset_ + cluster.meshletCount_; meshletIndex++)
			{
				const Meshlet& meshlet = meshlets_[meshletIndex];
				for (Uint32 triangleIndex = 0; triangleIndex < meshlet.triangleCount_; triangleIndex++)
				{
					Uint32 byteOffset = meshlet.triangleOffset_ + triangleIndex * 3;
					for (Int corner = 0; corner < 3; corner++)
					{
						outIndices.push_back(vertexIndices_[meshlet.vertexOffset_ + primitiveIndices_[byteOffset + corner]]);
					}
				}
			}
		}

		return outIndices.size() >= 3;
	}

	/**
	* [EN]
	* Reads each SubMesh's coarsest cluster into a CPU-side full-vertex/
	* index pair, compacted into its own local index space. Not part of
	* the bake/cache pipeline — called live off the already-loaded Crister.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 各 SubMesh の最粗クラスタを、自身のローカルインデックス空間に圧縮した
	* CPU 側のフル頂点/インデックス対へ読み出す。ベイク/キャッシュ
	* パイプラインの一部ではなく、ロード済みの Crister に対してその都度
	* 呼び出す。
	*/
	Bool Crister::SoftbodyCoarsestVertices(DynamicArray<Vertex>& outVertices, DynamicArray<Uint32>& outIndices)const
	{
		outVertices.clear();
		outIndices.clear();

		if (compressedVertices_.empty() || subMeshes_.empty())
		{
			return false;
		}

		/// [EN] Maps a QUANTISED bind-pose position to its slot in
		///      outVertices — unlike BakeCollision's remap (keyed by
		///      original global vertex index), this welds any two corners
		///      that land on the same position even if they came from
		///      different global indices (e.g. UV-seam-duplicated
		///      vertices). LOD simplification of the coarsest cluster can
		///      otherwise leave two distinct global indices at (numerically)
		///      the same position; feeding both into Jolt as separately
		///      connected vertices produces a zero-length edge, which
		///      corrupts SoftBodySharedSettings::CreateConstraints's
		///      Dijkstra-based CalculateClosestKinematic (crashes there —
		///      see PhysicsSystem::ResolveSoftbodies history). Quantised to
		///      a 1e-4 world-unit grid: fine enough that legitimately
		///      distinct vertices never collide, coarse enough to catch
		///      float round-trip noise between originally-identical
		///      positions.
		/// [JP] 量子化したバインドポーズ位置を outVertices 内のスロットへ
		///      対応付ける — BakeCollision のリマップ（元のグローバル頂点
		///      インデックスでキー）と異なり、これは元のグローバル
		///      インデックスが違っていても同じ位置に落ちる2つの角を溶接
		///      する（UV継ぎ目で複製された頂点など）。最粗クラスタの LOD
		///      簡略化は、（数値的に）同じ位置に異なる2つのグローバル
		///      インデックスを残すことがある。両方を別々に接続された頂点
		///      として Jolt に渡すと長さ0の辺ができ、
		///      SoftBodySharedSettings::CreateConstraints の Dijkstra ベース
		///      CalculateClosestKinematic を壊す（そこでクラッシュする —
		///      PhysicsSystem::ResolveSoftbodies の経緯参照）。1e-4
		///      ワールド単位グリッドへ量子化: 正当に別々の頂点が衝突しない
		///      程度に細かく、元々同一だった位置間の浮動小数往復誤差を
		///      拾える程度に粗い。
		auto quantisedPositionKey = [](const Vector3& position) -> Uint64
		{
			constexpr Float gridScale = 10000.0f;
			Int64 x = static_cast<Int64>(std::lround(position.x * gridScale));
			Int64 y = static_cast<Int64>(std::lround(position.y * gridScale));
			Int64 z = static_cast<Int64>(std::lround(position.z * gridScale));
			Uint64 hash = static_cast<Uint64>(x) * 73856093ull;
			hash ^= static_cast<Uint64>(y) * 19349663ull;
			hash ^= static_cast<Uint64>(z) * 83492791ull;
			return hash;
		};

		std::unordered_map<Uint64, Uint32> remap;

		for (const SubMesh& subMesh : subMeshes_)
		{
			if (subMesh.clusterCount_ == 0)
			{
				continue;
			}

			/// [EN] Coarsest cluster — same range BakeCollision's Proxy
			///      detail and the RT proxy geometry already use.
			/// [JP] 最粗クラスタ — BakeCollision の Proxy 詳細度や RT
			///      プロキシジオメトリが既に使っているのと同じ範囲。
			Uint32 clusterIndex = subMesh.clusterOffset_ + subMesh.clusterCount_ - 1;
			if (clusterIndex >= clusters_.size())
			{
				continue;
			}

			const Cluster& cluster = clusters_[clusterIndex];
			for (Uint32 meshletIndex = cluster.meshletOffset_; meshletIndex < cluster.meshletOffset_ + cluster.meshletCount_; meshletIndex++)
			{
				const Meshlet& meshlet = meshlets_[meshletIndex];
				for (Uint32 triangleIndex = 0; triangleIndex < meshlet.triangleCount_; triangleIndex++)
				{
					Uint32 byteOffset = meshlet.triangleOffset_ + triangleIndex * 3;
					for (Int corner = 0; corner < 3; corner++)
					{
						Uint32 globalIndex = vertexIndices_[meshlet.vertexOffset_ + primitiveIndices_[byteOffset + corner]];
						const CompressedVertex& compressed = compressedVertices_[globalIndex];

						Vertex vertex;
						vertex.position_ = DecodePosition(compressed);

						Uint64 key = quantisedPositionKey(vertex.position_);
						auto found = remap.find(key);
						if (found == remap.end())
						{
							Uint32 compactIndex = static_cast<Uint32>(outVertices.size());
							remap[key] = compactIndex;

							vertex.normal_ = DecodeOctahedralNormal(compressed.normal_);
							vertex.tangent_ = DecodeTangent(compressed);
							vertex.texcoord_ = DecodeTexcoord(compressed);
							outVertices.push_back(vertex);

							outIndices.push_back(compactIndex);
						}
						else
						{
							outIndices.push_back(found->second);
						}
					}
				}
			}
		}

		return outIndices.size() >= 3;
	}

	/**
	* [EN]
	* Quantises a single full-precision Vertex into the 16-byte GPU format
	* against the given position/texcoord dequantisation AABBs. Extracted
	* out of BakeMesh()'s per-vertex loop body; BakeMesh() calls this too,
	* so there is one source of truth for the quantisation math.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フル精度の Vertex 1 つを、指定された位置/UV の逆量子化 AABB に対して
	* 16 バイトの GPU フォーマットへ量子化する。BakeMesh() の頂点ループ
	* 本体から切り出したもの。BakeMesh() 自身もこれを呼ぶため、量子化
	* 計算の実装は 1 箇所に集約される。
	*/
	CompressedVertex Crister::EncodeVertex(const Vertex& vertex, const Vector3& positionMin, const Vector3& positionExtent, const Vector2& texcoordMin, const Vector2& texcoordExtent)
	{
		Uint32 quantizedX = QuantizeUnorm16((vertex.position_.x - positionMin.x) / positionExtent.x);
		Uint32 quantizedY = QuantizeUnorm16((vertex.position_.y - positionMin.y) / positionExtent.y);
		Uint32 quantizedZ = QuantizeUnorm16((vertex.position_.z - positionMin.z) / positionExtent.z);
		Uint32 quantizedU = QuantizeUnorm16((vertex.texcoord_.x - texcoordMin.x) / texcoordExtent.x);
		Uint32 quantizedV = QuantizeUnorm16((vertex.texcoord_.y - texcoordMin.y) / texcoordExtent.y);

		/// [JP] タンジェントは 8+7bit octahedral + 利き手符号 1bit。法線マップの
		///      接空間回転誤差は法線本体より知覚されにくいため粗くて足りる。
		Uint32 tangentOctahedral = EncodeOctahedralNormal(Vector3(vertex.tangent_.x, vertex.tangent_.y, vertex.tangent_.z));
		Uint32 tangentOctX8 = ((tangentOctahedral & 0xFFFF) * 255 + 32767) / 65535;
		Uint32 tangentOctY7 = ((tangentOctahedral >> 16) * 127 + 32767) / 65535;
		Uint32 tangentSign = vertex.tangent_.w >= 0.0f ? 1u : 0u;
		Uint32 quantizedTangent = tangentOctX8 | (tangentOctY7 << 8) | (tangentSign << 15);

		CompressedVertex compressed;
		compressed.positionXY_ = quantizedX | (quantizedY << 16);
		compressed.positionZTexU_ = quantizedZ | (quantizedU << 16);
		compressed.texVTangent_ = quantizedV | (quantizedTangent << 16);
		compressed.normal_ = EncodeOctahedralNormal(vertex.normal_);
		return compressed;
	}

	/**
	* [EN]
	* Inverse of EncodeVertex: dequantises a CompressedVertex back into a
	* full-precision Vertex, against this Crister's own quantisation AABBs.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* EncodeVertex の逆。CompressedVertex を、この Crister 自身の量子化 AABB
	* に対してフル精度の Vertex へ逆量子化する。
	*/
	Vertex Crister::DecodeVertex(const CompressedVertex& compressed)const
	{
		Vertex vertex;
		vertex.position_ = DecodePosition(compressed);
		vertex.normal_ = DecodeOctahedralNormal(compressed.normal_);
		vertex.tangent_ = DecodeTangent(compressed);
		vertex.texcoord_ = DecodeTexcoord(compressed);
		return vertex;
	}

	/**
	* [EN]
	* Packs a unit direction into the 16+16-bit octahedral encoding
	* (Shader/Normal.hlsli::OctNormalEncode's CPU-side counterpart). Static
	* for the same reason as EncodeVertex: SoftbodyMesh calls this indirectly
	* via EncodeVertex with its own bounds, not this Crister's.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単位方向ベクトルを 16+16bit のオクタヘドラル符号化へ詰める
	* （Shader/Normal.hlsli::OctNormalEncode の CPU 側対応）。EncodeVertex と
	* 同じ理由で static — SoftbodyMesh が EncodeVertex 経由で、この Crister
	* とは別の自身の境界を使って間接的に呼ぶため。
	*/
	Uint32 Crister::EncodeOctahedralNormal(Vector3 direction)
	{
		Float sum = std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z);
		if (sum < 1e-8f)
		{
			direction = Vector3(0, 0, 1);
			sum = 1.0f;
		}

		Float x = direction.x / sum;
		Float y = direction.y / sum;
		if (direction.z < 0.0f)
		{
			Float foldedX = (1.0f - std::abs(y)) * (x >= 0.0f ? 1.0f : -1.0f);
			Float foldedY = (1.0f - std::abs(x)) * (y >= 0.0f ? 1.0f : -1.0f);
			x = foldedX;
			y = foldedY;
		}

		return QuantizeUnorm16(x * 0.5f + 0.5f) | (QuantizeUnorm16(y * 0.5f + 0.5f) << 16);
	}

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
	Vector3 Crister::DecodeOctahedralNormal(Uint32 packed)const
	{
		Float ex = static_cast<Float>(packed & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
		Float ey = static_cast<Float>(packed >> 16) / 65535.0f * 2.0f - 1.0f;

		Vector3 n(ex, ey, 1.0f - std::abs(ex) - std::abs(ey));
		if (n.z < 0.0f)
		{
			Float nx = (1.0f - std::abs(n.y)) * (n.x >= 0.0f ? 1.0f : -1.0f);
			Float ny = (1.0f - std::abs(n.x)) * (n.y >= 0.0f ? 1.0f : -1.0f);
			n.x = nx;
			n.y = ny;
		}
		n.Normalize();
		return n;
	}

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
	Bool Crister::ApplyAxisConversion(const Matrix& deltaBasis, Bool flipWinding, const std::filesystem::path& cristerPath)
	{
		if (compressedVertices_.empty())
		{
			return false;
		}

		Float determinant = deltaBasis.Determinant();
		Bool isMirror = determinant < 0.0f;
		Float tangentSign = isMirror ? -1.0f : 1.0f;

		/// [EN] Decode every quantised vertex back to full precision, then
		///      transform position/normal/tangent in place. Texcoord is
		///      unaffected by an axis-convention change.
		/// [JP] 量子化済みの全頂点をフル精度へデコードし、位置/法線/タンジェントを
		///      その場で変換する。テクスチャ座標は軸コンベンション変更の影響を
		///      受けない。
		DynamicArray<Vertex> transformedVertices(compressedVertices_.size());
		for (Size vertexIndex = 0; vertexIndex < compressedVertices_.size(); vertexIndex++)
		{
			const CompressedVertex& compressed = compressedVertices_[vertexIndex];
			Vertex& vertex = transformedVertices[vertexIndex];

			Vertex decoded = DecodeVertex(compressed);
			vertex.position_ = Vector3::Transform(decoded.position_, deltaBasis);
			vertex.texcoord_ = decoded.texcoord_;
			vertex.normal_ = Vector3::Transform(decoded.normal_, deltaBasis);

			Vector3 tangentXyz = Vector3::Transform(Vector3(decoded.tangent_.x, decoded.tangent_.y, decoded.tangent_.z), deltaBasis);
			vertex.tangent_ = Vector4(tangentXyz.x, tangentXyz.y, tangentXyz.z, decoded.tangent_.w * tangentSign);

			if (!compressedSkinVertices_.empty())
			{
				DecodeSkin(compressedSkinVertices_[vertexIndex], vertex.joints_, vertex.weights_);
			}
		}

		for (Node& node : nodes_)
		{
			ConvertRotationByBasis(node.rotation_, deltaBasis);
			ConvertPositionByBasis(node.translation_, deltaBasis);
		}
		CumulateTransforms();

		for (Skin& skin : skins_)
		{
			for (Matrix& inverseBindMatrix : skin.inverseBindMatrices_)
			{
				ConvertMatrixByBasis(inverseBindMatrix, deltaBasis);
			}
		}

		for (PunctualLight& light : lights_)
		{
			ConvertPositionByBasis(light.position_, deltaBasis);
			ConvertPositionByBasis(light.direction_, deltaBasis);
			light.direction_.Normalize();
		}

		for (MeshletBound& bound : meshletBounds_)
		{
			ConvertPositionByBasis(bound.center_, deltaBasis);
			ConvertPositionByBasis(bound.coneAxis_, deltaBasis);
		}

		if (flipWinding)
		{
			Size triangleCount = primitiveIndices_.size() / 3;
			for (Size triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
			{
				std::swap(primitiveIndices_[triangleIndex * 3 + 1], primitiveIndices_[triangleIndex * 3 + 2]);
			}
		}

		/// [EN] Reuse BakeMesh()'s existing quantisation pipeline verbatim: it
		///      recomputes positionMin_/positionExtent_/texcoordMin_/
		///      texcoordExtent_ from vertices_ and re-populates
		///      compressedVertices_/compressedSkinVertices_.
		/// [JP] BakeMesh() の既存の量子化パイプラインをそのまま再利用する:
		///      vertices_ から positionMin_/positionExtent_/texcoordMin_/
		///      texcoordExtent_ を再計算し、compressedVertices_/
		///      compressedSkinVertices_ を再構築する。
		vertices_ = std::move(transformedVertices);
		BakeMesh();

		BinaryOutputArchive archive;
		Serialize(archive);
		if (!archive.Write(String(cristerPath.string())))
		{
			return false;
		}

		return true;
	}

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
	Bool Crister::ApplyTransformConversion(Vector3 position, Vector3 rotation, Vector3 scale, Vector3 pivot, const std::filesystem::path& cristerPath)
	{
		if (compressedVertices_.empty())
		{
			return false;
		}

		/// [EN] Away-from-zero clamp: a zero/near-zero axis would bake a
		///      singular linearBasis, making normalBasis (its inverse-transpose)
		///      undefined.
		/// [JP] ゼロから離す方向へのクランプ: 軸が 0/0 近傍だと linearBasis が
		///      特異になり、その逆転置である normalBasis が定義できなくなる。
		Vector3 clampedScale(
			std::abs(scale.x) < 0.0001f ? std::copysign(0.0001f, scale.x) : scale.x,
			std::abs(scale.y) < 0.0001f ? std::copysign(0.0001f, scale.y) : scale.y,
			std::abs(scale.z) < 0.0001f ? std::copysign(0.0001f, scale.z) : scale.z);

		Matrix linearBasis = Matrix::CreateScale(clampedScale.x, clampedScale.y, clampedScale.z) * Matrix::CreateFromYawPitchRoll(ToRadians(rotation.y), ToRadians(rotation.x), ToRadians(rotation.z));
		Matrix normalBasis = linearBasis.Invert().Transpose();
		Matrix fullTransform = Matrix::CreateTranslation(-pivot) * linearBasis * Matrix::CreateTranslation(pivot + position);

		Float tangentSign = linearBasis.Determinant() < 0.0f ? -1.0f : 1.0f;

		DynamicArray<Vertex> transformedVertices(compressedVertices_.size());
		for (Size vertexIndex = 0; vertexIndex < compressedVertices_.size(); vertexIndex++)
		{
			const CompressedVertex& compressed = compressedVertices_[vertexIndex];
			Vertex& vertex = transformedVertices[vertexIndex];

			Vertex decoded = DecodeVertex(compressed);
			vertex.position_ = Vector3::Transform(decoded.position_, fullTransform);
			vertex.texcoord_ = decoded.texcoord_;
			vertex.normal_ = Vector3::TransformNormal(decoded.normal_, normalBasis);
			vertex.normal_.Normalize();

			Vector3 tangentXyz = Vector3::TransformNormal(Vector3(decoded.tangent_.x, decoded.tangent_.y, decoded.tangent_.z), linearBasis);
			tangentXyz.Normalize();
			vertex.tangent_ = Vector4(tangentXyz.x, tangentXyz.y, tangentXyz.z, decoded.tangent_.w * tangentSign);

			if (!compressedSkinVertices_.empty())
			{
				DecodeSkin(compressedSkinVertices_[vertexIndex], vertex.joints_, vertex.weights_);
			}
		}

		if (defaultStage_ >= 0 && static_cast<Size>(defaultStage_) < stages_.size())
		{
			for (Int rootNodeIndex : stages_[defaultStage_].nodes_)
			{
				if (rootNodeIndex < 0 || static_cast<Size>(rootNodeIndex) >= nodes_.size())
				{
					continue;
				}

				Node& node = nodes_[rootNodeIndex];
				Matrix localScale = Matrix::CreateScale(node.scale_.x, node.scale_.y, node.scale_.z);
				Matrix localRotation = Matrix::CreateFromQuaternion(node.rotation_);
				Matrix localTranslation = Matrix::CreateTranslation(node.translation_.x, node.translation_.y, node.translation_.z);
				Matrix newLocal = localScale * localRotation * localTranslation * fullTransform;

				newLocal.Decompose(node.scale_, node.rotation_, node.translation_);
			}
		}
		CumulateTransforms();

		Matrix inverseFullTransform = fullTransform.Invert();
		for (Skin& skin : skins_)
		{
			for (Matrix& inverseBindMatrix : skin.inverseBindMatrices_)
			{
				inverseBindMatrix = inverseFullTransform * inverseBindMatrix;
			}
		}

		for (PunctualLight& light : lights_)
		{
			light.position_ = Vector3::Transform(light.position_, fullTransform);
			light.direction_ = Vector3::TransformNormal(light.direction_, linearBasis);
			light.direction_.Normalize();
		}

		Float radiusScale = Max(Max(std::abs(clampedScale.x), std::abs(clampedScale.y)), std::abs(clampedScale.z));
		for (MeshletBound& bound : meshletBounds_)
		{
			bound.center_ = Vector3::Transform(bound.center_, fullTransform);
			bound.coneAxis_ = Vector3::TransformNormal(bound.coneAxis_, linearBasis);
			bound.coneAxis_.Normalize();
			bound.radius_ *= radiusScale;
		}

		vertices_ = std::move(transformedVertices);
		BakeMesh();

		BinaryOutputArchive archive;
		Serialize(archive);
		if (!archive.Write(String(cristerPath.string())))
		{
			return false;
		}

		return true;
	}

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
	void Crister::Upload(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap)
	{
		HRESULT hr{ S_OK };

		DirectX::ResourceUploadBatch resourceUpload(device);
		resourceUpload.Begin();

		/// [EN] Capture upload context so cluster pages can stream in and out
		///      after load (these engine objects outlive every Crister).
		/// [JP] ロード後にクラスタページを出し入れできるようアップロード
		///      コンテキストを保持する（全 Crister より長寿命のオブジェクト）。
		device_ = device;
		uploadQueue_ = cmdQueue;
		bindlessHeap_ = heap;

		/// [EN] Derive each cluster's page ranges from its meshlet slice.
		///      buildMeshletsFromIndices appends sequentially, so a cluster's
		///      meshlets occupy contiguous slices of vertexIndices_ and
		///      primitiveIndices_; LOD>=1 clusters also own a contiguous vertex
		///      range (QEM appends fresh vertex copies per LOD). LOD 0 clusters
		///      reference the original vertex pool instead.
		/// [JP] 各クラスタのページレンジを meshlet 列から導出する。
		///      buildMeshletsFromIndices は逐次追加なので、クラスタの meshlet は
		///      vertexIndices_ / primitiveIndices_ の連続スライスを占有する。
		///      LOD>=1 クラスタは専用の連続頂点範囲も所有する（QEM が LOD ごとに
		///      新頂点を追加）。LOD 0 クラスタは元の頂点プールを参照する。
		streamingGeometry_.assign(clusters_.size(), StreamingGeometry{});
		poolVertexEnd_ = 0;
		for (Size clusterIndex = 0; clusterIndex < clusters_.size(); clusterIndex++)
		{
			const Cluster& cluster = clusters_[clusterIndex];
			StreamingGeometry& page = streamingGeometry_[clusterIndex];
			if (cluster.meshletCount_ == 0)
			{
				continue;
			}

			const Meshlet& first = meshlets_[cluster.meshletOffset_];
			const Meshlet& last = meshlets_[cluster.meshletOffset_ + cluster.meshletCount_ - 1];
			page.vertexIndexBegin_ = first.vertexOffset_;
			page.vertexIndexEnd_ = last.vertexOffset_ + last.vertexCount_;
			page.primitiveBegin_ = first.triangleOffset_;
			page.primitiveEnd_ = last.triangleOffset_ + last.triangleCount_ * 3;
			page.ownsVertices_ = cluster.lodLevel_ >= 1;

			Uint32 minVertex = 0xFFFFFFFF;
			Uint32 maxVertex = 0;
			for (Uint32 index = page.vertexIndexBegin_; index < page.vertexIndexEnd_; index++)
			{
				minVertex = Min(minVertex, vertexIndices_[index]);
				maxVertex = Max(maxVertex, vertexIndices_[index]);
			}
			if (page.ownsVertices_)
			{
				page.vertexBegin_ = minVertex;
				page.vertexEnd_ = maxVertex + 1;
			}
			else
			{
				page.vertexBegin_ = 0;
				page.vertexEnd_ = maxVertex + 1;
				poolVertexEnd_ = Max(poolVertexEnd_, maxVertex + 1);
			}
		}

		/// [EN] Pin the pages that must never leave VRAM: the coarsest cluster
		///      of every SubMesh (guaranteed fallback LOD) and, for skinned
		///      SubMeshes, LOD 0 (their only drawable LOD). The vertex pool is
		///      pinned when any pinned LOD 0 cluster references it.
		/// [JP] VRAM から出してはいけないページをピン留めする: 各 SubMesh の
		///      最粗クラスタ（フォールバック LOD の保証）と、スキンド SubMesh の
		///      LOD 0（唯一描画可能な LOD）。ピン留めされた LOD 0 クラスタが
		///      あれば頂点プールもピン留めする。
		for (const SubMesh& subMesh : subMeshes_)
		{
			if (subMesh.clusterCount_ == 0)
			{
				continue;
			}
			streamingGeometry_[subMesh.clusterOffset_ + subMesh.clusterCount_ - 1].pinned_ = true;
			if (subMesh.skinIndex_ >= 0)
			{
				streamingGeometry_[subMesh.clusterOffset_].pinned_ = true;
			}
		}
		for (Size clusterIndex = 0; clusterIndex < streamingGeometry_.size(); clusterIndex++)
		{
			if (streamingGeometry_[clusterIndex].pinned_ && !streamingGeometry_[clusterIndex].ownsVertices_)
			{
				poolPinned_ = true;
			}
		}

		/// [EN] Skinning attributes for the pool range (skinned SubMeshes are
		///      LOD 0 only, so pool-sized coverage is sufficient). Always
		///      resident when skins exist — skinned LOD 0 is pinned anyway.
		/// [JP] プール範囲分のスキニング属性（スキンド SubMesh は LOD 0 のみ
		///      なのでプールサイズで足りる）。スキンがあれば常に常駐 — どのみち
		///      スキンド LOD 0 はピン留めされている。
		if (!skins_.empty() && poolVertexEnd_ > 0)
		{
			hr = CreateStaticBufferUnbounded(device, resourceUpload, compressedSkinVertices_.data(), poolVertexEnd_, sizeof(CompressedSkinVertex), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, skinVertexResource_.ReleaseAndGetAddressOf());
			SC_HR_CHECK(hr, "スキンバーテックスバッファの生成に失敗しました");
			skinVertexBufferIndex_ = CreateStructuredShaderResourceView(device, heap, skinVertexResource_.Get(), poolVertexEnd_, sizeof(CompressedSkinVertex));
		}

		/// [EN] RT proxy geometry: flatten each SubMesh's COARSEST cluster into
		///      a compact vertex set + 32-bit triangle list. Reflections trace
		///      against this low-poly proxy, so RT never forces full-detail
		///      geometry to stay resident. Positions are decoded on the CPU with
		///      the same math as the shader decode.
		/// [JP] RT プロキシジオメトリ: 各 SubMesh の最粗クラスタをコンパクトな
		///      頂点集合 + 32bit 三角形リストへ展開する。反射はこの低ポリ
		///      プロキシに対してトレースするため、RT のためにフル詳細ジオメトリを
		///      常駐させ続ける必要がない。位置はシェーダデコードと同じ計算で
		///      CPU デコードする。
		DynamicArray<Uint32> flatTriangleIndices;
		DynamicArray<CompressedVertex> raytracingVertices;
		DynamicArray<Vector3> raytracingPositions;
		DynamicArray<CompressedSkinVertex> raytracingSkinVertices;
		DynamicArray<Vector3> raytracingMorphDeltas;
		Bool buildRaytracingSkinVertices = skins_.size() == 1 && !compressedSkinVertices_.empty();
		std::unordered_map<Uint32, Uint32> raytracingRemap;
		for (SubMesh& subMesh : subMeshes_)
		{
			if (subMesh.clusterCount_ == 0)
			{
				continue;
			}

			/// [EN] RT uses LOD 0 (full detail): reflections show the mesh at its
			///      best quality, at the cost of keeping the RT flat-index /
			///      position buffers fully resident. Rasterisation still streams.
			/// [JP] RT は LOD 0（フル詳細）を使う: 反射は最高品質のメッシュで
			///      描かれる。代償として RT のフラットインデックス/位置バッファは
			///      常に全量常駐する。ラスタ側のストリーミングは従来どおり。
			const Cluster& cluster = clusters_[subMesh.clusterOffset_];

			/// [EN] Encounter order for this SubMesh's compact vertices is
			///      contiguous ascending indices into raytracingVertices (globalIndex
			///      never collides across SubMeshes, since each owns a
			///      disjoint range of vertices_) — so [raytracingVertexOffset_,
			///      raytracingVertexOffset_ + raytracingVertexCount_) below is valid, and
			///      submeshMorphDeltas[target] fills in that same order.
			/// [JP] この SubMesh のコンパクト頂点の出現順は raytracingVertices への
			///      連続した昇順インデックスになる(globalIndex は SubMesh間で
			///      衝突しない、各 SubMesh が vertices_ の互いに素な範囲を
			///      持つため) — そのため下の [raytracingVertexOffset_,
			///      raytracingVertexOffset_ + raytracingVertexCount_) は正しく、
			///      submeshMorphDeltas[target] も同じ順序で埋まる。
			subMesh.raytracingVertexOffset_ = static_cast<Uint32>(raytracingVertices.size());
			DynamicArray<DynamicArray<Vector3>> submeshMorphDeltas(subMesh.morphs_.size());

			for (Uint32 meshletIndex = cluster.meshletOffset_; meshletIndex < cluster.meshletOffset_ + cluster.meshletCount_; meshletIndex++)
			{
				const Meshlet& meshlet = meshlets_[meshletIndex];
				for (Uint32 triangleIndex = 0; triangleIndex < meshlet.triangleCount_; triangleIndex++)
				{
					Uint32 byteOffset = meshlet.triangleOffset_ + triangleIndex * 3;
					for (Int corner = 0; corner < 3; corner++)
					{
						Uint32 globalIndex = vertexIndices_[meshlet.vertexOffset_ + primitiveIndices_[byteOffset + corner]];
						auto found = raytracingRemap.find(globalIndex);
						if (found == raytracingRemap.end())
						{
							Uint32 compactIndex = static_cast<Uint32>(raytracingVertices.size());
							raytracingRemap[globalIndex] = compactIndex;

							const CompressedVertex& compressed = compressedVertices_[globalIndex];
							raytracingVertices.push_back(compressed);

							/// [EN] A source asset with a degenerate/non-finite
							///      quantisation AABB (positionMin_/positionExtent_)
							///      decodes to a NaN/Inf position here even though
							///      the compressed bit pattern itself is well-formed.
							///      That NaN reaches the BLAS's position buffer,
							///      producing a NaN bounding box the traversal
							///      hardware can never cull against - every ray
							///      descends into it instead of skipping it, which
							///      is a no-page-fault DispatchRays hang rather than
							///      a crash. Falling back to the origin keeps this
							///      mesh's triangle count/indexing intact (so BLAS
							///      build still succeeds) while giving the BVH a
							///      finite, if wrong-looking, box.
							/// [JP] 量子化 AABB(positionMin_/positionExtent_)が退化/非有限な
							///      ソースアセットは、圧縮ビットパターン自体は
							///      正常でもここで NaN/Inf の位置へデコードされる。
							///      その NaN は BLAS の位置バッファへそのまま渡り、
							///      走査ハードウェアが絶対にカリングできない NaN
							///      境界ボックスを生む - 全てのレイがスキップされず
							///      そこへ降りていくため、クラッシュではなく
							///      ページフォルト無しの DispatchRays ハングになる。
							///      原点へフォールバックすれば、このメッシュの
							///      三角形数/インデックス構造は保ったまま
							///      (BLAS 構築を成功させたまま)、見た目はおかしくとも
							///      有限な境界ボックスを BVH に与えられる。
							Vector3 position = DecodePosition(compressed);
							if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
							{
								if (!degenerateRaytracingPositionLogged_)
								{
									SC_LOG_WARNING("RT プロキシの頂点位置が非有限です。ソースアセットの量子化 AABB が壊れている可能性があります。");
									degenerateRaytracingPositionLogged_ = true;
								}
								position = Vector3::Zero;
							}

							raytracingPositions.push_back(position);
							flatTriangleIndices.push_back(compactIndex);

							if (buildRaytracingSkinVertices)
							{
								if (subMesh.skinIndex_ >= 0 && globalIndex < compressedSkinVertices_.size())
								{
									raytracingSkinVertices.push_back(compressedSkinVertices_[globalIndex]);
								}
								else
								{
									raytracingSkinVertices.push_back(CompressedSkinVertex{});
								}
							}

							if (!subMesh.morphs_.empty() && globalIndex >= subMesh.vertexOffset_)
							{
								Uint32 localVertexIndex = globalIndex - subMesh.vertexOffset_;
								for (Size targetIndex = 0; targetIndex < subMesh.morphs_.size(); targetIndex++)
								{
									const DynamicArray<Vector3>& deltas = subMesh.morphs_[targetIndex].positionDeltas_;
									submeshMorphDeltas[targetIndex].push_back(localVertexIndex < deltas.size() ? deltas[localVertexIndex] : Vector3::Zero);
								}
							}
						}
						else
						{
							flatTriangleIndices.push_back(found->second);
						}
					}
				}
			}

			subMesh.raytracingVertexCount_ = static_cast<Uint32>(raytracingVertices.size()) - subMesh.raytracingVertexOffset_;

			if (!subMesh.morphs_.empty())
			{
				subMesh.raytracingMorphDeltaOffset_ = static_cast<Uint32>(raytracingMorphDeltas.size());
				for (const DynamicArray<Vector3>& targetDeltas : submeshMorphDeltas)
				{
					raytracingMorphDeltas.insert(raytracingMorphDeltas.end(), targetDeltas.begin(), targetDeltas.end());
				}
			}
		}

		if (!flatTriangleIndices.empty())
		{
			hr = CreateStaticBufferUnbounded(device, resourceUpload, raytracingVertices.data(), raytracingVertices.size(), sizeof(CompressedVertex), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, vertexResource_.ReleaseAndGetAddressOf());
			SC_HR_CHECK(hr, "レイトレーシング用バーテックスバッファの生成に失敗しました");
			vertexBufferIndex_ = CreateStructuredShaderResourceView(device, heap, vertexResource_.Get(), static_cast<Uint>(raytracingVertices.size()), sizeof(CompressedVertex));
			proxyVertexCount_ = static_cast<Uint32>(raytracingVertices.size());

			hr = CreateStaticBufferUnbounded(device, resourceUpload, raytracingPositions.data(), raytracingPositions.size(), sizeof(Vector3), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, positionResource_.ReleaseAndGetAddressOf());
			SC_HR_CHECK(hr, "レイトレーシング用ポジションバッファの生成に失敗しました");

			hr = CreateStaticBufferUnbounded(device, resourceUpload, flatTriangleIndices.data(), flatTriangleIndices.size(), sizeof(Uint32), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, indexResource_.ReleaseAndGetAddressOf());
			SC_HR_CHECK(hr, "レイトレーシング用インデックスバッファの生成に失敗しました");
			triangleIndexCount_ = static_cast<Uint32>(flatTriangleIndices.size());
			indexBufferIndex_ = CreateStructuredShaderResourceView(device, heap, indexResource_.Get(), triangleIndexCount_, sizeof(Uint32));

			if (buildRaytracingSkinVertices && raytracingSkinVertices.size() == raytracingVertices.size())
			{
				hr = CreateStaticBufferUnbounded(device, resourceUpload, raytracingSkinVertices.data(), raytracingSkinVertices.size(), sizeof(CompressedSkinVertex), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, raytracingSkinVertexResource_.ReleaseAndGetAddressOf());
				SC_HR_CHECK(hr, "レイトレーシング用スキンバーテックスバッファの生成に失敗しました");
			}

			if (!raytracingMorphDeltas.empty())
			{
				hr = CreateStaticBufferUnbounded(device, resourceUpload, raytracingMorphDeltas.data(), raytracingMorphDeltas.size(), sizeof(Vector3), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, raytracingMorphDeltaResource_.ReleaseAndGetAddressOf());
				SC_HR_CHECK(hr, "レイトレーシング用モーフデルタバッファの生成に失敗しました");
			}
		}

		/// [EN] Raster morph support: unlike the RT proxy's morph delta pool
		///      above, this is baked straight from Morph::positionDeltas_
		///      (no compaction/remap — every SubMesh's original vertexCount_
		///      deltas per target, back to back) since the raster path
		///      already has its own per-vertex remap (vertexMorphSource_)
		///      to resolve any streamed LOD's vertex back to this buffer's
		///      indexing. See ApplyMorphBlend in Model.hlsli.
		/// [JP] ラスタのモーフ対応: 上の RT プロキシ用モーフデルタプールと
		///      違い、Morph::positionDeltas_ からそのまま焼き込む(圧縮/
		///      リマップ無し — 各 SubMesh のオリジナル vertexCount_ 個の
		///      デルタをターゲットごとに連続で)。ラスタ経路はストリーム
		///      されたどの LOD の頂点もこのバッファの番号付けへ逆引きする
		///      独自の頂点リマップ(vertexMorphSource_)を既に持つため。
		///      Model.hlsli の ApplyMorphBlend 参照。
		DynamicArray<Vector3> morphDeltas;
		for (SubMesh& subMesh : subMeshes_)
		{
			if (subMesh.morphs_.empty())
			{
				continue;
			}

			subMesh.morphDeltaOffset_ = static_cast<Uint32>(morphDeltas.size());
			for (const Morph& morph : subMesh.morphs_)
			{
				morphDeltas.insert(morphDeltas.end(), morph.positionDeltas_.begin(), morph.positionDeltas_.end());
			}
		}

		if (!morphDeltas.empty())
		{
			hr = CreateStaticBufferUnbounded(device, resourceUpload, morphDeltas.data(), morphDeltas.size(), sizeof(Vector3), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, morphDeltaResource_.ReleaseAndGetAddressOf());
			SC_HR_CHECK(hr, "ラスタ用モーフデルタバッファの生成に失敗しました");
			morphDeltaBufferIndex_ = CreateStructuredShaderResourceView(device, heap, morphDeltaResource_.Get(), static_cast<Uint>(morphDeltas.size()), sizeof(Vector3));

			hr = CreateStaticBufferUnbounded(device, resourceUpload, vertexMorphSource_.data(), vertexMorphSource_.size(), sizeof(Uint32), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, vertexMorphSourceResource_.ReleaseAndGetAddressOf());
			SC_HR_CHECK(hr, "頂点モーフソースバッファの生成に失敗しました");
			vertexMorphSourceBufferIndex_ = CreateStructuredShaderResourceView(device, heap, vertexMorphSourceResource_.Get(), static_cast<Uint>(vertexMorphSource_.size()), sizeof(Uint32));
		}

		/// [EN] Set up per-texture streaming state (see StreamingTexture) from
		///      the BC7-compressed bitmaps baked by BakeBitmap(). No GPU work
		///      here — MakeTextureMipResident() below uploads mips on demand,
		///      starting with each texture's pinned coarsest mip.
		/// [JP] BakeBitmap() が焼いた BC7 圧縮ビットマップから、テクスチャごとの
		///      ストリーミング状態を準備する（StreamingTexture 参照）。ここでは
		///      GPU 作業を行わない — 下の MakeTextureMipResident() が各テクスチャの
		///      ピン留め最粗ミップから始めてオンデマンドでアップロードする。
		streamingTextures_.assign(bitmaps_.size(), StreamingTexture{});
		for (Size textureIndex = 0; textureIndex < bitmaps_.size(); textureIndex++)
		{
			const Bitmap& bitmap = bitmaps_[textureIndex];
			if (bitmap.width_ <= 0 || bitmap.height_ <= 0 || bitmap.cacheData_.empty() || bitmap.mipCount_ <= 0)
			{
				continue;
			}

			/// [EN] Only mips that are legal as a STANDALONE single-mip BC7 resource
			///      can be streamed: D3D12 requires a block-compressed resource with
			///      MipLevels == 1 to have both dimensions a non-zero multiple of 4
			///      (sub-4 sizes such as the 2x2/1x1 tail BakeBitmap generates are
			///      only legal as inner levels of a longer chain). Streaming the
			///      whole chain would therefore fail to create the coarsest levels
			///      and leave the material with no SRV at all. Clamp mipCount_ to
			///      the leading run of legal mips, so the pinned fallback is 4x4
			///      rather than a flat 1x1 — and never sub-4.
			/// [JP] 「単一ミップの独立した BC7 リソース」として合法なミップだけを
			///      ストリーミング対象にする: D3D12 はブロック圧縮リソースの
			///      MipLevels == 1 のとき、幅・高さの両方が 0 でない 4 の倍数である
			///      ことを要求する(BakeBitmap が生成する末尾の 2x2/1x1 のような
			///      4 未満の寸法は、より長いチェーンの内側の段としてのみ合法)。
			///      チェーン全体をストリーミング対象にすると最粗段の生成に失敗し、
			///      マテリアルに SRV が 1 つも無い状態になってしまう。合法なミップが
			///      続く範囲で mipCount_ を打ち切り、ピン留めフォールバックが
			///      単色の 1x1 ではなく 4x4 になるようにする(4 未満は作らない)。
			Uint32 streamableMipCount = 0;
			Int mipWidth = bitmap.width_;
			Int mipHeight = bitmap.height_;
			while (streamableMipCount < static_cast<Uint32>(bitmap.mipCount_) && mipWidth >= 4 && mipHeight >= 4 && mipWidth % 4 == 0 && mipHeight % 4 == 0)
			{
				streamableMipCount++;
				mipWidth /= 2;
				mipHeight /= 2;
			}
			if (streamableMipCount == 0)
			{
				SC_LOG_WARNING("テクスチャ {} の寸法({}x{})が BC7 の単一ミップリソースとして不正なため、ストリーミング対象外にします。", textureIndex, bitmap.width_, bitmap.height_);
				continue;
			}

			StreamingTexture& streamingTexture = streamingTextures_[textureIndex];
			streamingTexture.mipCount_ = streamableMipCount;
			streamingTexture.topResidentMip_ = streamingTexture.mipCount_;
			streamingTexture.valid_ = true;
		}

		std::future<void> uploadFinished;
		{
			auto queueLock = cmdQueue->AcquireLock();
			uploadFinished = resourceUpload.End(cmdQueue->GetCommandQueue());
		}
		uploadFinished.wait();

		/// [EN] Register with the streaming bookkeeping, then bring the pinned
		///      geometry pages and texture mips in so the model is immediately
		///      drawable at its fallback LOD/resolution. Finer pages/mips
		///      stream in on demand from ModelRenderer::Gather.
		/// [JP] ストリーミング管理へ登録し、ピン留めジオメトリページとテクスチャ
		///      ミップをアップロードしてフォールバック LOD/解像度で即描画可能に
		///      する。より細かいページ/ミップは ModelRenderer::Gather から
		///      オンデマンドで常駐する。
		if (!std::ranges::contains(streamingRegistry_, this))
		{
			streamingRegistry_.push_back(this);
		}
		for (Size clusterIndex = 0; clusterIndex < streamingGeometry_.size(); clusterIndex++)
		{
			if (streamingGeometry_[clusterIndex].pinned_)
			{
				MakeClusterResident(static_cast<Uint32>(clusterIndex));
			}
		}
		for (Size textureIndex = 0; textureIndex < streamingTextures_.size(); textureIndex++)
		{
			if (streamingTextures_[textureIndex].valid_)
			{
				MakeTextureMipResident(static_cast<Uint32>(textureIndex), streamingTextures_[textureIndex].mipCount_ - 1);
			}
		}
	}

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
	const DynamicArray<Stage>& Crister::Stages()const
	{
		return stages_; 
	}

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
	const DynamicArray<Node>& Crister::Nodes()const
	{
		return nodes_;
	}

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
	const DynamicArray<PunctualLight>& Crister::Lights()const
	{
		return lights_;
	}

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
	const DynamicArray<SubMesh>& Crister::SubMeshes()const
	{
		return subMeshes_;
	}

	/**
	* [EN]
	* Returns every Surface (PBR factors + KHR_materials_* extensions +
	* texture indices) parsed from the source glTF.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ソース glTF から解析された、全 Surface（PBR ファクタ +
	* KHR_materials_* 拡張 + テクスチャインデックス）を返す。
	*/
	const DynamicArray<Surface>& Crister::Surfaces()const
	{
		return surfaces_;
	}

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
	const DynamicArray<Skin>& Crister::Skins()const
	{
		return skins_;
	}

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
	const DynamicArray<Cluster>& Crister::Clusters()const
	{
		return clusters_;
	}

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
	Int Crister::DefaultStage()const
	{
		return defaultStage_;
	}

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
	Int Crister::FindNodeIndex(const std::string& name)const
	{
		for (Size nodeIndex = 0; nodeIndex < nodes_.size(); nodeIndex++)
		{
			if (nodes_[nodeIndex].name_ == name)
			{
				return static_cast<Int>(nodeIndex);
			}
		}
		return -1;
	}

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
	Vector3 Crister::PositionMin()const
	{
		return positionMin_;
	}

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
	Vector3 Crister::PositionExtent()const
	{
		return positionExtent_;
	}

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
	Vector2 Crister::TexcoordMin()const
	{
		return texcoordMin_;
	}

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
	Vector2 Crister::TexcoordExtent()const
	{
		return texcoordExtent_;
	}

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
	D3D12_GPU_VIRTUAL_ADDRESS Crister::PositionBufferAddress()const
	{
		return positionResource_ ? positionResource_->GetGPUVirtualAddress() : 0;
	}

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
	Uint32 Crister::VertexCount()const
	{
		return proxyVertexCount_;
	}

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
	D3D12_GPU_VIRTUAL_ADDRESS Crister::IndexBufferAddress()const
	{
		return indexResource_ ? indexResource_->GetGPUVirtualAddress() : 0;
	}

	/**
	* [EN]
	* Triangle count of the flat index buffer IndexBufferAddress() points to.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* IndexBufferAddress() が指すフラットインデックスバッファの三角形数。
	*/
	Uint32 Crister::IndexCount()const
	{
		return triangleIndexCount_;
	}

	/**
	* [EN]
	* Maps a glTF image index (as stored in Surface) to the bindless
	* heap index of the uploaded GPU texture. Returns 0xFFFFFFFF when
	* not present.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* glTF の image インデックス（Surface に格納されている値）を、
	* アップロード済み GPU テクスチャの bindless ヒープインデックスに
	* 変換する。無ければ 0xFFFFFFFF。
	*/
	Uint Crister::TextureBindlessIndex(Uint32 textureIndex)const
	{
		if (textureIndex >= streamingTextures_.size())
		{
			return 0xFFFFFFFF;
		}
		const StreamingTexture& streamingTexture = streamingTextures_[textureIndex];
		if (!streamingTexture.valid_ || streamingTexture.topResidentMip_ >= streamingTexture.mipCount_)
		{
			return 0xFFFFFFFF;
		}
		Bool isPinnedMip = streamingTexture.topResidentMip_ == streamingTexture.mipCount_ - 1;
		return isPinnedMip ? streamingTexture.pinnedMip_.bindlessIndex_ : streamingTexture.currentMip_.bindlessIndex_;
	}

	/**
	* [EN]
	* Whether any texture's resident bindless index has changed (via
	* MakeTextureMipResident/EvictTextureMip) since the last
	* ClearMaterialsDirty().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 前回の ClearMaterialsDirty() 以降に、いずれかのテクスチャの常駐
	* バインドレスインデックスが(MakeTextureMipResident/EvictTextureMip
	* 経由で)変わったか。
	*/
	Bool Crister::IsMaterialsDirty()const
	{
		return materialsDirty_;
	}

	/**
	* [EN]
	* Clears the flag IsMaterialsDirty() reports.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* IsMaterialsDirty() が報告するフラグをクリアする。
	*/
	void Crister::ClearMaterialsDirty()const
	{
		materialsDirty_ = false;
	}

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
	Uint Crister::VertexBufferIndex()const
	{
		return vertexBufferIndex_;
	}

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
	Uint Crister::IndexBufferIndex()const
	{
		return indexBufferIndex_;
	}

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
	Uint Crister::SkinVertexBufferIndex()const
	{
		return skinVertexBufferIndex_;
	}

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
	Uint Crister::MorphDeltaBufferIndex()const
	{
		return morphDeltaBufferIndex_;
	}

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
	Uint Crister::VertexMorphSourceBufferIndex()const
	{
		return vertexMorphSourceBufferIndex_;
	}

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
	Bool Crister::IsProxySkinned()const
	{
		return raytracingSkinVertexResource_ != nullptr;
	}

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
	D3D12_GPU_VIRTUAL_ADDRESS Crister::ProxySkinVertexBufferAddress()const
	{
		return raytracingSkinVertexResource_ ? raytracingSkinVertexResource_->GetGPUVirtualAddress() : 0;
	}

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
	D3D12_GPU_VIRTUAL_ADDRESS Crister::ProxyMorphDeltaBufferAddress()const
	{
		return raytracingMorphDeltaResource_ ? raytracingMorphDeltaResource_->GetGPUVirtualAddress() : 0;
	}

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
	void Crister::CopyMorph(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* destination)const
	{
		if (!positionResource_ || !destination || proxyVertexCount_ == 0)
		{
			return;
		}
		cmdList->CopyBufferRegion(destination, 0, positionResource_.Get(), 0, static_cast<UINT64>(proxyVertexCount_) * sizeof(Vector3));
	}

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
	void Crister::MakeClusterResident(Uint32 clusterIndex)
	{
		StreamingGeometry& page = streamingGeometry_[clusterIndex];
		const Cluster& cluster = clusters_[clusterIndex];
		if (page.resident_ || cluster.meshletCount_ == 0 || !device_)
		{
			return;
		}

		if (!page.ownsVertices_)
		{
			MakePoolResident();
			poolResidentReferences_++;
		}

		/// [EN] Rebase the cluster's meshlets to page-local offsets. The mesh
		///      shaders are untouched: ModelInstance simply points at the page's
		///      buffers and a page-local meshlet offset.
		/// [JP] クラスタの meshlet をページローカルオフセットへリベースする。
		///      メッシュシェーダは無変更: ModelInstance がページのバッファと
		///      ページローカルの meshlet オフセットを指すだけ。
		DynamicArray<Meshlet> localMeshlets(cluster.meshletCount_);
		DynamicArray<MeshletBound> localBounds(cluster.meshletCount_);
		for (Uint32 meshletIndex = 0; meshletIndex < cluster.meshletCount_; meshletIndex++)
		{
			Meshlet meshlet = meshlets_[cluster.meshletOffset_ + meshletIndex];
			meshlet.vertexOffset_ -= page.vertexIndexBegin_;
			meshlet.triangleOffset_ -= page.primitiveBegin_;
			localMeshlets[meshletIndex] = meshlet;
			localBounds[meshletIndex] = meshletBounds_[cluster.meshletOffset_ + meshletIndex];
		}

		DynamicArray<Uint32> localVertexIndices(page.vertexIndexEnd_ - page.vertexIndexBegin_);
		for (Uint32 index = page.vertexIndexBegin_; index < page.vertexIndexEnd_; index++)
		{
			Uint32 value = vertexIndices_[index];
			localVertexIndices[index - page.vertexIndexBegin_] = page.ownsVertices_ ? value - page.vertexBegin_ : value;
		}

		Uint32 primitiveSize = page.primitiveEnd_ - page.primitiveBegin_;
		Uint32 alignedSize = (primitiveSize + 3) & ~3u;
		DynamicArray<Uint8> localPrimitives(alignedSize, 0);
		memcpy(localPrimitives.data(), primitiveIndices_.data() + page.primitiveBegin_, primitiveSize);

		DynamicArray<CompressedVertex> localVertices;
		if (page.ownsVertices_)
		{
			localVertices.resize(page.vertexEnd_ - page.vertexBegin_);
			for (Uint32 vertexIndex = page.vertexBegin_; vertexIndex < page.vertexEnd_; vertexIndex++)
			{
				localVertices[vertexIndex - page.vertexBegin_] = compressedVertices_[vertexIndex];
			}
		}

		DirectX::ResourceUploadBatch resourceUpload(device_);
		resourceUpload.Begin();
		HRESULT hr = CreateStaticBufferUnbounded(device_, resourceUpload, localMeshlets.data(), localMeshlets.size(), sizeof(Meshlet), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, page.meshletResource_.ReleaseAndGetAddressOf());
		SC_HR_CHECK(hr, "メッシュレットバッファの生成に失敗しました");
		page.meshletBufferIndex_ = CreateStructuredShaderResourceView(device_, bindlessHeap_, page.meshletResource_.Get(), cluster.meshletCount_, sizeof(Meshlet));

		hr = CreateStaticBufferUnbounded(device_, resourceUpload, localBounds.data(), localBounds.size(), sizeof(MeshletBound), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, page.meshletBoundResource_.ReleaseAndGetAddressOf());
		SC_HR_CHECK(hr, "メッシュレットバウンドバッファの生成に失敗しました");
		page.meshletBoundBufferIndex_ = CreateStructuredShaderResourceView(device_, bindlessHeap_, page.meshletBoundResource_.Get(), cluster.meshletCount_, sizeof(MeshletBound));

		hr = CreateStaticBufferUnbounded(device_, resourceUpload, localVertexIndices.data(), localVertexIndices.size(), sizeof(Uint32), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, page.vertexIndicesResource_.ReleaseAndGetAddressOf());
		SC_HR_CHECK(hr, "頂点インデックスバッファの生成に失敗しました");
		page.vertexIndicesBufferIndex_ = CreateStructuredShaderResourceView(device_, bindlessHeap_, page.vertexIndicesResource_.Get(), static_cast<Uint>(localVertexIndices.size()), sizeof(Uint32));

		hr = CreateStaticBufferUnbounded(device_, resourceUpload, localPrimitives.data(), alignedSize, 1, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, page.primitiveIndicesResource_.ReleaseAndGetAddressOf());
		SC_HR_CHECK(hr, "プリミティブインデックスバッファの生成に失敗しました");
		{
			Uint index = bindlessHeap_->AllocateIndex();

			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Buffer.FirstElement = 0;
			shaderResourceViewDesc.Buffer.NumElements = alignedSize / 4;
			shaderResourceViewDesc.Buffer.StructureByteStride = 0;
			shaderResourceViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			device_->CreateShaderResourceView(page.primitiveIndicesResource_.Get(), &shaderResourceViewDesc, bindlessHeap_->CPUHandle(index));

			page.primitiveIndicesBufferIndex_ = index;
		}

		if (page.ownsVertices_)
		{
			hr = CreateStaticBufferUnbounded(device_, resourceUpload, localVertices.data(), localVertices.size(), sizeof(CompressedVertex), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, page.vertexResource_.ReleaseAndGetAddressOf());
			SC_HR_CHECK(hr, "頂点バッファの生成に失敗しました");
			page.vertexBufferIndex_ = CreateStructuredShaderResourceView(device_, bindlessHeap_, page.vertexResource_.Get(), static_cast<Uint>(localVertices.size()), sizeof(CompressedVertex));
		}

		std::future<void> finished;
		{
			auto queueLock = uploadQueue_->AcquireLock();
			finished = resourceUpload.End(uploadQueue_->GetCommandQueue());
		}
		finished.wait();

		page.sizeBytes_ =
			static_cast<Uint64>(cluster.meshletCount_) * (sizeof(Meshlet) + sizeof(MeshletBound)) +
			localVertexIndices.size() * sizeof(Uint32) +
			alignedSize +
			localVertices.size() * sizeof(CompressedVertex);
		totalResidentGeometryBytes_ += page.sizeBytes_;
		page.resident_ = true;
	}

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
	void Crister::MakeTextureMipResident(Uint32 textureIndex, Uint32 targetMip)
	{
		if (textureIndex >= streamingTextures_.size())
		{
			return;
		}

		StreamingTexture& streamingTexture = streamingTextures_[textureIndex];
		if (!streamingTexture.valid_ || !device_)
		{
			return;
		}

		/// [JP] 既に同等以上に細かいミップが常駐しているなら何もしない。
		Uint32 mipIndex = Min(targetMip, streamingTexture.mipCount_ - 1);
		if (mipIndex >= streamingTexture.topResidentMip_)
		{
			return;
		}

		const Bitmap& bitmap = bitmaps_[textureIndex];

		Uint32 mipWidth = 0;
		Uint32 mipHeight = 0;
		Uint64 rowPitch = 0;
		Uint64 slicePitch = 0;
		Uint64 byteOffset = 0;
		ComputeTextureMipLayout(bitmap, mipIndex, mipWidth, mipHeight, rowPitch, slicePitch, byteOffset);

		D3D12_RESOURCE_DESC textureDesc{};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = mipWidth;
		textureDesc.Height = mipHeight;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_BC7_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		Microsoft::WRL::ComPtr<ID3D12Resource> newResource;
		HRESULT hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(newResource.ReleaseAndGetAddressOf()));

		/// [JP] ミップ1枚の失敗はモデル全体を巻き込んでクラッシュさせるほどではない。
		///      警告を出してこのミップ抜きで続行する(topResidentMip_ は進めない)。
		if (FAILED(hr))
		{
			SC_LOG_WARNING("テクスチャ {} のミップ {} のアップロードに失敗しました(HRESULT=0x{:08X}, {}x{})。このミップ抜きで続行します。", textureIndex, mipIndex, static_cast<Uint32>(hr), mipWidth, mipHeight);
			return;
		}
#ifdef _DEBUG
		newResource->SetName(L"Crister_TextureMip");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(newResource.Get());
#endif

		D3D12_SUBRESOURCE_DATA subresource{};
		subresource.pData = bitmap.cacheData_.data() + byteOffset;
		subresource.RowPitch = static_cast<LONG_PTR>(rowPitch);
		subresource.SlicePitch = static_cast<LONG_PTR>(slicePitch);

		DirectX::ResourceUploadBatch resourceUpload(device_);
		resourceUpload.Begin();
		resourceUpload.Upload(newResource.Get(), 0, &subresource, 1);
		resourceUpload.Transition(newResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		std::future<void> finished;
		{
			auto queueLock = uploadQueue_->AcquireLock();
			finished = resourceUpload.End(uploadQueue_->GetCommandQueue());
		}
		finished.wait();

		Uint newIndex = bindlessHeap_->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_BC7_UNORM;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		device_->CreateShaderResourceView(newResource.Get(), &shaderResourceViewDesc, bindlessHeap_->CPUHandle(newIndex));

		/// [JP] 中間ミップは常駐させ続けない: ピン留めミップ(最粗)を初めて
		///      アップロードする1回だけ pinnedMip_ へ、それ以外は currentMip_ を
		///      置き換える(古い currentMip_ があれば解放してから差し替える)。
		if (mipIndex == streamingTexture.mipCount_ - 1)
		{
			streamingTexture.pinnedMip_.resource_ = newResource;
			streamingTexture.pinnedMip_.bindlessIndex_ = newIndex;
			streamingTexture.pinnedMip_.sizeBytes_ = slicePitch;
		}
		else
		{
			if (streamingTexture.currentMip_.resource_)
			{
				bindlessHeap_->FreeIndex(streamingTexture.currentMip_.bindlessIndex_);
				/// [EN] The mip being replaced is the one the in-flight frames are
				///      still sampling - this upgrade is driven by worldScale, so it
				///      fires on the very frame the Actor's Scale changes. Dropping
				///      the last reference here would destroy the texture out from
				///      under the GPU.
				/// [JP] 置き換えられるミップは、インフライトのフレームがまだ
				///      サンプリングしている当のもの — この昇格は worldScale で
				///      駆動されるため、Actor の Scale を変えたまさにそのフレームで
				///      発火する。ここで最後の参照を落とすと、GPU が使っている
				///      最中にテクスチャを破棄することになる。
				bindlessHeap_->DeferRelease(streamingTexture.currentMip_.resource_);
				totalResidentTextureBytes_ -= streamingTexture.currentMip_.sizeBytes_;
			}
			streamingTexture.currentMip_.resource_ = newResource;
			streamingTexture.currentMip_.bindlessIndex_ = newIndex;
			streamingTexture.currentMip_.sizeBytes_ = slicePitch;
		}

		totalResidentTextureBytes_ += slicePitch;
		streamingTexture.topResidentMip_ = mipIndex;
		materialsDirty_ = true;
	}

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
	void Crister::MakePoolResident()
	{
		if (poolResident_ || poolVertexEnd_ == 0 || !device_)
		{
			return;
		}

		DirectX::ResourceUploadBatch resourceUpload(device_);
		resourceUpload.Begin();
		HRESULT hr = CreateStaticBufferUnbounded(device_, resourceUpload, compressedVertices_.data(), poolVertexEnd_, sizeof(CompressedVertex), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, poolResource_.ReleaseAndGetAddressOf());
		SC_HR_CHECK(hr, "頂点プールバッファの生成に失敗しました");
		poolBufferIndex_ = CreateStructuredShaderResourceView(device_, bindlessHeap_, poolResource_.Get(), poolVertexEnd_, sizeof(CompressedVertex));
		std::future<void> finished;
		{
			auto queueLock = uploadQueue_->AcquireLock();
			finished = resourceUpload.End(uploadQueue_->GetCommandQueue());
		}
		finished.wait();

		poolSizeBytes_ = static_cast<Uint64>(poolVertexEnd_) * sizeof(CompressedVertex);
		totalResidentGeometryBytes_ += poolSizeBytes_;
		poolResident_ = true;
	}

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
	void Crister::EvictCluster(Uint32 clusterIndex)
	{
		StreamingGeometry& page = streamingGeometry_[clusterIndex];
		if (!page.resident_ || page.pinned_)
		{
			return;
		}

		bindlessHeap_->FreeIndex(page.meshletBufferIndex_);
		bindlessHeap_->FreeIndex(page.meshletBoundBufferIndex_);
		bindlessHeap_->FreeIndex(page.vertexIndicesBufferIndex_);
		bindlessHeap_->FreeIndex(page.primitiveIndicesBufferIndex_);
		bindlessHeap_->DeferRelease(page.meshletResource_);
		bindlessHeap_->DeferRelease(page.meshletBoundResource_);
		bindlessHeap_->DeferRelease(page.vertexIndicesResource_);
		bindlessHeap_->DeferRelease(page.primitiveIndicesResource_);
		page.meshletResource_.Reset();
		page.meshletBoundResource_.Reset();
		page.vertexIndicesResource_.Reset();
		page.primitiveIndicesResource_.Reset();
		if (page.ownsVertices_)
		{
			bindlessHeap_->FreeIndex(page.vertexBufferIndex_);
			bindlessHeap_->DeferRelease(page.vertexResource_);
			page.vertexResource_.Reset();
		}
		else
		{
			poolResidentReferences_--;
		}

		totalResidentGeometryBytes_ -= page.sizeBytes_;
		page.sizeBytes_ = 0;
		page.resident_ = false;
	}

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
	void Crister::EvictTextureMip(Uint32 textureIndex)
	{
		if (textureIndex >= streamingTextures_.size())
		{
			return;
		}

		StreamingTexture& streamingTexture = streamingTextures_[textureIndex];
		if (!streamingTexture.valid_ || streamingTexture.topResidentMip_ >= streamingTexture.mipCount_ - 1)
		{
			return;
		}

		bindlessHeap_->FreeIndex(streamingTexture.currentMip_.bindlessIndex_);
		bindlessHeap_->DeferRelease(streamingTexture.currentMip_.resource_);
		streamingTexture.currentMip_.resource_.Reset();
		streamingTexture.currentMip_.bindlessIndex_ = 0xFFFFFFFF;
		totalResidentTextureBytes_ -= streamingTexture.currentMip_.sizeBytes_;
		streamingTexture.currentMip_.sizeBytes_ = 0;
		streamingTexture.topResidentMip_ = streamingTexture.mipCount_ - 1;
		materialsDirty_ = true;
	}

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
	void Crister::EvictPool()
	{
		if (!poolResident_ || poolPinned_ || poolResidentReferences_ > 0)
		{
			return;
		}
		bindlessHeap_->FreeIndex(poolBufferIndex_);
		bindlessHeap_->DeferRelease(poolResource_);
		poolResource_.Reset();
		totalResidentGeometryBytes_ -= poolSizeBytes_;
		poolSizeBytes_ = 0;
		poolResident_ = false;
	}

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
	void Crister::TouchCluster(Uint32 clusterIndex, Uint64 frame)
	{
		streamingGeometry_[clusterIndex].lastUsedFrame_ = frame;
		if (!streamingGeometry_[clusterIndex].ownsVertices_)
		{
			poolLastUsedFrame_ = frame;
		}
	}

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
	void Crister::TouchTexture(Uint32 textureIndex, Uint64 frame)
	{
		if (textureIndex >= streamingTextures_.size())
		{
			return;
		}
		streamingTextures_[textureIndex].lastUsedFrame_ = frame;
	}

	/**
	* [EN]
	* Whether the cluster's page is currently GPU-resident.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* クラスタのページが現在 GPU に常駐しているか。
	*/
	Bool Crister::IsClusterResident(Uint32 clusterIndex)const
	{
		return streamingGeometry_[clusterIndex].resident_;
	}

	/**
	* [EN]
	* Whether the texture has streamed all the way in to mip 0 (its finest).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* テクスチャがミップ 0（最も細かい）まで完全にストリームインしているか。
	*/
	Bool Crister::IsTextureResident(Uint32 textureIndex)const
	{
		if (textureIndex >= streamingTextures_.size())
		{
			return false;
		}
		return streamingTextures_[textureIndex].topResidentMip_ == 0;
	}

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
	void Crister::EvictClusterBudget(Uint64 currentFrame)
	{
		if (totalResidentGeometryBytes_ <= geometryBudgetBytes_)
		{
			return;
		}

		struct Candidate
		{
			Crister* crister_;
			Uint32 clusterIndex_;
			Uint64 lastUsedFrame_;
		};
		DynamicArray<Candidate> candidates;
		for (Crister* crister : streamingRegistry_)
		{
			for (Size clusterIndex = 0; clusterIndex < crister->streamingGeometry_.size(); clusterIndex++)
			{
				const StreamingGeometry& page = crister->streamingGeometry_[clusterIndex];
				if (page.resident_ && !page.pinned_ && page.lastUsedFrame_ + evictAgeFrames_ <= currentFrame)
				{
					candidates.push_back({ crister, static_cast<Uint32>(clusterIndex), page.lastUsedFrame_ });
				}
			}
		}

		std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b)
		{
			return a.lastUsedFrame_ < b.lastUsedFrame_;
		});

		for (const Candidate& candidate : candidates)
		{
			if (totalResidentGeometryBytes_ <= geometryBudgetBytes_)
			{
				break;
			}
			candidate.crister_->EvictCluster(candidate.clusterIndex_);
		}

		for (Crister* crister : streamingRegistry_)
		{
			if (totalResidentGeometryBytes_ <= geometryBudgetBytes_)
			{
				break;
			}
			if (crister->poolLastUsedFrame_ + evictAgeFrames_ <= currentFrame)
			{
				crister->EvictPool();
			}
		}
	}

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
	void Crister::EvictTextureBudget(Uint64 currentFrame)
	{
		if (totalResidentTextureBytes_ <= textureBudgetBytes_)
		{
			return;
		}

		struct Candidate
		{
			Crister* crister_;
			Uint32 textureIndex_;
			Uint64 lastUsedFrame_;
		};
		DynamicArray<Candidate> candidates;
		for (Crister* crister : streamingRegistry_)
		{
			for (Size textureIndex = 0; textureIndex < crister->streamingTextures_.size(); textureIndex++)
			{
				const StreamingTexture& streamingTexture = crister->streamingTextures_[textureIndex];
				if (streamingTexture.valid_ && streamingTexture.topResidentMip_ < streamingTexture.mipCount_ - 1 && streamingTexture.lastUsedFrame_ + evictAgeFrames_ <= currentFrame)
				{
					candidates.push_back({ crister, static_cast<Uint32>(textureIndex), streamingTexture.lastUsedFrame_ });
				}
			}
		}

		std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b)
		{
			return a.lastUsedFrame_ < b.lastUsedFrame_;
		});

		for (const Candidate& candidate : candidates)
		{
			if (totalResidentTextureBytes_ <= textureBudgetBytes_)
			{
				break;
			}
			candidate.crister_->EvictTextureMip(candidate.textureIndex_);
		}
	}

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
	Uint Crister::ClusterVertexBufferIndex(Uint32 clusterIndex)const
	{
		const StreamingGeometry& page = streamingGeometry_[clusterIndex];
		return page.ownsVertices_ ? page.vertexBufferIndex_ : poolBufferIndex_;
	}

	/**
	* [EN]
	* Bindless SRV of the cluster page's meshlet buffer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* クラスタページの meshlet バッファの bindless SRV。
	*/
	Uint Crister::ClusterMeshletBufferIndex(Uint32 clusterIndex)const
	{
		return streamingGeometry_[clusterIndex].meshletBufferIndex_;
	}

	/**
	* [EN]
	* Bindless SRV of the cluster page's meshlet bound buffer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* クラスタページの meshlet バウンドバッファの bindless SRV。
	*/
	Uint Crister::ClusterMeshletBoundBufferIndex(Uint32 clusterIndex)const
	{
		return streamingGeometry_[clusterIndex].meshletBoundBufferIndex_;
	}

	/**
	* [EN]
	* Bindless SRV of the cluster page's vertex indices buffer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* クラスタページの頂点インデックスバッファの bindless SRV。
	*/
	Uint Crister::ClusterVertexIndicesBufferIndex(Uint32 clusterIndex)const
	{
		return streamingGeometry_[clusterIndex].vertexIndicesBufferIndex_;
	}

	/**
	* [EN]
	* Bindless SRV of the cluster page's primitive indices buffer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* クラスタページのプリミティブインデックスバッファの bindless SRV。
	*/
	Uint Crister::ClusterPrimitiveIndicesBufferIndex(Uint32 clusterIndex)const
	{
		return streamingGeometry_[clusterIndex].primitiveIndicesBufferIndex_;
	}

	/**
	* [EN]
	* Whether this cluster page owns its own vertex slice
	* (streamingGeometry_[clusterIndex].ownsVertices_) rather than
	* referencing the shared LOD 0 pool. Own-page vertex indices are
	* rebased to page-local numbering by MakeClusterResident, so they are
	* NOT valid indices into Crister::vertexMorphSource_/
	* morphDeltaResource_ (which use the crister-wide numbering the shared
	* pool preserves) — callers populating a raster morph instance must
	* check this and leave morph fields zeroed
	* (ModelInstanceData::morphTargetCount_ == 0) for any cluster where
	* this returns true.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このクラスタページが共有 LOD 0 プールを参照するのではなく、自前の
	* 頂点スライスを持つか(streamingGeometry_[clusterIndex].ownsVertices_)。
	* 自前ページの頂点インデックスは MakeClusterResident によってページ
	* ローカルな番号へリベースされるため、Crister::vertexMorphSource_/
	* morphDeltaResource_(共有プールが保つ Crister 全体の番号付けを使う)
	* への有効なインデックスでは【ない】— ラスタのモーフ用インスタンスを
	* 組み立てる側はこれを確認し、true が返るクラスタではモーフフィールドを
	* ゼロのまま(ModelInstanceData::morphTargetCount_ == 0)にすること。
	*/
	Bool Crister::StandaloneVertices(Uint32 clusterIndex)const
	{
		return streamingGeometry_[clusterIndex].ownsVertices_;
	}

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
	Uint32 Crister::TextureFinestMip(Uint32 textureIndex)const
	{
		if (textureIndex >= streamingTextures_.size())
		{
			return 0;
		}
		return streamingTextures_[textureIndex].topResidentMip_;
	}

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
	Uint32 Crister::TextureDesiredMip(Uint32 textureIndex, Float worldScale, Float pixelsPerUnit)const
	{
		if (textureIndex >= streamingTextures_.size() || !streamingTextures_[textureIndex].valid_)
		{
			return 0;
		}

		/// [EN] worldScale is the transform's scale factor, NOT the model's size:
		///      an unscaled 10-unit building and an unscaled 0.1-unit prop both
		///      report 1.0. Using it alone underestimates screen coverage by the
		///      model's extent and picks a far too coarse mip (this is what made
		///      everything look soft). Scale it by the quantisation AABB's
		///      diagonal, which is the model's actual world-space size, to get
		///      the real on-screen pixel span.
		///      texelsPerScreenPixel is then how many mip-0 texels crowd into one
		///      screen pixel; each doubling costs exactly one mip level. Assumes
		///      the texture maps roughly once across the model — there is no
		///      per-mesh texel-density data to do better, so bias toward sharp
		///      (round down) rather than risk over-blurring.
		/// [JP] worldScale はトランスフォームの倍率でありモデルの大きさではない:
		///      等倍の 10 ユニットの建物も等倍の 0.1 ユニットの小物も 1.0 を返す。
		///      これだけで判断するとモデルの実寸分だけ画面被覆を過小評価し、
		///      粗すぎるミップを選んでしまう(これが全体が眠く見えた原因)。
		///      モデルの実ワールドサイズである量子化 AABB の対角長を掛けて、
		///      実際の画面上のピクセル幅を求める。
		///      texelsPerScreenPixel は画面 1 ピクセルに詰め込まれる mip 0 の
		///      テクセル数で、2 倍になるごとにちょうど 1 ミップ粗くできる。
		///      テクスチャがモデル全体におよそ 1 回貼られる前提(メッシュごとの
		///      テクセル密度データが無いためこれ以上詰められない)。ぼやけ過ぎる
		///      リスクを避けるため切り捨てて鮮明側に倒す。
		Vector3 extent = positionExtent_;
		Float modelSize = Max(Max(extent.x, extent.y), extent.z);
		Float screenPixels = Max(worldScale * modelSize * pixelsPerUnit, 1.0f);
		Float textureWidth = static_cast<Float>(bitmaps_[textureIndex].width_);

		Uint32 desiredMip = 0;
		Float texelsPerScreenPixel = textureWidth / screenPixels;
		if (texelsPerScreenPixel > 1.0f)
		{
			desiredMip = static_cast<Uint32>(std::log2(texelsPerScreenPixel));
		}

		Uint32 mipCount = streamingTextures_[textureIndex].mipCount_;
		return mipCount == 0 ? 0 : Min(desiredMip, mipCount - 1);
	}

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
	void Crister::CumulateTransforms()
	{
		if (defaultStage_ < 0 || static_cast<Size>(defaultStage_) >= stages_.size())
		{
			return;
		}

		std::stack<Matrix> parentGlobalTransforms;
		std::function<void(Int)> traverse = [&](Int nodeIndex)
			{
				if (nodeIndex < 0 || static_cast<Size>(nodeIndex) >= nodes_.size())
				{
					return;
				}

				Node& node = nodes_[nodeIndex];
				Matrix scale = Matrix::CreateScale(node.scale_.x, node.scale_.y, node.scale_.z);
				Matrix rotation = Matrix::CreateFromQuaternion(node.rotation_);
				Matrix translation = Matrix::CreateTranslation(node.translation_.x, node.translation_.y, node.translation_.z);

				Matrix localTransform = scale * rotation * translation;
				node.globalTransform_ = localTransform * parentGlobalTransforms.top();

				for (Int childIndex : node.children_)
				{
					parentGlobalTransforms.push(node.globalTransform_);
					traverse(childIndex);
					parentGlobalTransforms.pop();
				}
			};

		for (Int nodeIndex : stages_[defaultStage_].nodes_)
		{
			parentGlobalTransforms.push(Matrix::Identity);
			traverse(nodeIndex);
			parentGlobalTransforms.pop();
		}
	}

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
	Uint Crister::CreateStructuredShaderResourceView(ID3D12Device* device, BindlessHeap* heap, ID3D12Resource* resource, Uint elementCount, Uint stride)
	{
		Uint index = heap->AllocateIndex();

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Buffer.FirstElement = 0;
		shaderResourceViewDesc.Buffer.NumElements = elementCount;
		shaderResourceViewDesc.Buffer.StructureByteStride = stride;
		shaderResourceViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		device->CreateShaderResourceView(resource, &shaderResourceViewDesc, heap->CPUHandle(index));

		return index;
	}

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
	HRESULT Crister::CreateStaticBufferUnbounded(ID3D12Device* device, DirectX::ResourceUploadBatch& resourceUpload, const void* data, Uint64 count, Uint64 stride, D3D12_RESOURCE_STATES afterState, ID3D12Resource** outResource)
	{
		const Uint64 sizeInBytes = count * stride;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeInBytes;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(resource.GetAddressOf()));
		if (FAILED(hr))
		{
			return hr;
		}
#ifdef _DEBUG
		resource->SetName(L"Crister_StaticBufferUnbounded");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(resource.Get());
#endif

		D3D12_SUBRESOURCE_DATA subresourceData{ data, 0, 0 };
		resourceUpload.Upload(resource.Get(), 0, &subresourceData, 1);
		resourceUpload.Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, afterState);

		*outResource = resource.Detach();
		return S_OK;
	}

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
	void Crister::ComputeTextureMipLayout(const Bitmap& bitmap, Uint32 mipIndex, Uint32& outWidth, Uint32& outHeight, Uint64& outRowPitch, Uint64& outSlicePitch, Uint64& outByteOffset)
	{
		Uint64 offset = 0;
		Int mipWidth = bitmap.width_;
		Int mipHeight = bitmap.height_;
		for (Uint32 mip = 0; mip < mipIndex; mip++)
		{
			Uint64 blockWidth = Max<Uint64>(1, (static_cast<Uint64>(mipWidth) + 3) / 4);
			Uint64 blockHeight = Max<Uint64>(1, (static_cast<Uint64>(mipHeight) + 3) / 4);
			offset += blockWidth * 16 * blockHeight;
			mipWidth = Max(1, mipWidth / 2);
			mipHeight = Max(1, mipHeight / 2);
		}

		Uint64 blockWidth = Max<Uint64>(1, (static_cast<Uint64>(mipWidth) + 3) / 4);
		Uint64 blockHeight = Max<Uint64>(1, (static_cast<Uint64>(mipHeight) + 3) / 4);
		outWidth = static_cast<Uint32>(mipWidth);
		outHeight = static_cast<Uint32>(mipHeight);
		outRowPitch = blockWidth * 16;
		outSlicePitch = outRowPitch * blockHeight;
		outByteOffset = offset;
	}

	/**
	* [EN]
	* Dequantises just the position field of a CompressedVertex, against this
	* Crister's own positionMin_/positionExtent_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CompressedVertex の position フィールドだけを、この Crister 自身の
	* positionMin_/positionExtent_ に対して逆量子化する。
	*/
	Vector3 Crister::DecodePosition(const CompressedVertex& compressed)const
	{
		return positionMin_ + Vector3(
			static_cast<Float>(compressed.positionXY_ & 0xFFFF) / 65535.0f * positionExtent_.x,
			static_cast<Float>(compressed.positionXY_ >> 16) / 65535.0f * positionExtent_.y,
			static_cast<Float>(compressed.positionZTexU_ & 0xFFFF) / 65535.0f * positionExtent_.z);
	}

	/**
	* [EN]
	* Dequantises just the texcoord field of a CompressedVertex, against this
	* Crister's own texcoordMin_/texcoordExtent_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CompressedVertex の texcoord フィールドだけを、この Crister 自身の
	* texcoordMin_/texcoordExtent_ に対して逆量子化する。
	*/
	Vector2 Crister::DecodeTexcoord(const CompressedVertex& compressed)const
	{
		return texcoordMin_ + Vector2(
			static_cast<Float>(compressed.positionZTexU_ >> 16) / 65535.0f * texcoordExtent_.x,
			static_cast<Float>(compressed.texVTangent_ & 0xFFFF) / 65535.0f * texcoordExtent_.y);
	}

	/**
	* [EN]
	* Dequantises just the tangent field (xyz direction + handedness sign in
	* .w) of a CompressedVertex.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CompressedVertex の tangent フィールド（xyz 方向 + .w の利き手符号）だけ
	* を逆量子化する。
	*/
	Vector4 Crister::DecodeTangent(const CompressedVertex& compressed)const
	{
		Uint32 tangentBits = compressed.texVTangent_ >> 16;
		Uint32 octX16 = static_cast<Uint32>(static_cast<Float>(tangentBits & 0xFF) / 255.0f * 65535.0f + 0.5f);
		Uint32 octY16 = static_cast<Uint32>(static_cast<Float>((tangentBits >> 8) & 0x7F) / 127.0f * 65535.0f + 0.5f);
		Float sign = (tangentBits >> 15) ? 1.0f : -1.0f;

		Vector3 xyz = DecodeOctahedralNormal(octX16 | (octY16 << 16));
		return Vector4(xyz.x, xyz.y, xyz.z, sign);
	}

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
	void Crister::DecodeSkin(const CompressedSkinVertex& compressed, XmUint4& outJoints, Vector4& outWeights)const
	{
		outJoints.x = compressed.jointsXY_ & 0xFFFF;
		outJoints.y = compressed.jointsXY_ >> 16;
		outJoints.z = compressed.jointsZW_ & 0xFFFF;
		outJoints.w = compressed.jointsZW_ >> 16;

		outWeights = Vector4(static_cast<Float>(compressed.weights_ & 0xFF), static_cast<Float>((compressed.weights_ >> 8) & 0xFF), static_cast<Float>((compressed.weights_ >> 16) & 0xFF), static_cast<Float>(compressed.weights_ >> 24)) / 255.0f;

		Float weightSum = outWeights.x + outWeights.y + outWeights.z + outWeights.w;
		outWeights /= Max(weightSum, 1e-6f);
	}

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
	void Crister::ConvertPositionByBasis(Vector3& vector, const Matrix& basis)const
	{
		vector = Vector3::Transform(vector, basis);
	}

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
	void Crister::ConvertRotationByBasis(Quaternion& quaternion, const Matrix& basis)const
	{
		Matrix rotation = Matrix::CreateFromQuaternion(quaternion);
		Matrix converted = basis.Transpose() * rotation * basis;
		quaternion = Quaternion::CreateFromRotationMatrix(converted);
	}

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
	void Crister::ConvertMatrixByBasis(Matrix& matrix, const Matrix& basis)const
	{
		matrix = basis.Transpose() * matrix * basis;
	}

	/**
	* [EN]
	* Quantises a [0,1] float to a 16-bit UNORM, clamping out-of-range input
	* first. Shared by EncodeVertex (position/texcoord) and
	* EncodeOctahedralNormal.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* [0,1] の float を 16bit UNORM へ量子化する。範囲外の入力は先にクランプ
	* する。EncodeVertex（position/texcoord）と EncodeOctahedralNormal が共有する。
	*/
	Uint32 Crister::QuantizeUnorm16(Float value01)
	{
		Float clamped = value01 < 0.0f ? 0.0f : (value01 > 1.0f ? 1.0f : value01);
		return static_cast<Uint32>(clamped * 65535.0f + 0.5f);
	}

	/**
	* [EN]
	* Quantises a [0,1] float to an 8-bit UNORM, clamping out-of-range input
	* first. Used by BakeMesh for skin weights.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* [0,1] の float を 8bit UNORM へ量子化する。範囲外の入力は先にクランプ
	* する。BakeMesh がスキンウェイトに使う。
	*/
	Uint32 Crister::QuantizeUnorm8(Float value01)
	{
		Float clamped = value01 < 0.0f ? 0.0f : (value01 > 1.0f ? 1.0f : value01);
		return static_cast<Uint32>(clamped * 255.0f + 0.5f);
	}

}
