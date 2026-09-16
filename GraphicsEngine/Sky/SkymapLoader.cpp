#include <GraphicsEngine/Sky/SkymapLoader.h>
#include <GraphicsEngine/Sky/SkymapCache.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	Handle<Skymap> SkymapLoader::Load(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, String filePath)
	{
		Handle<Skymap> handle = pool_.Create();
		Skymap* skymap = pool_.Get(handle);
		if (!skymap)
		{
			return Handle<Skymap>::null();
		}

		skymap->handle_ = handle;

		std::string extension = std::filesystem::path(filePath.str()).extension().string();
		std::ranges::transform(extension, extension.begin(), [](Uchar c) { return static_cast<Char>(std::tolower(c)); });

		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		Uint width = 0;
		Uint height = 0;
		Uint rowPitch = 0;
		Uint slicePitch = 0;
		const void* pixels = nullptr;

		DirectX::ScratchImage image;
		DynamicArray<Uint8> cachePixels;
		Bool fromCache = (extension == ".skymap");

		if (fromCache)
		{
			SkymapCacheHeader header{};
			if (!ReadSkymapCache(filePath, header, cachePixels))
			{
				SC_LOG_ERROR("スカイマップキャッシュのロードに失敗しました: {}", filePath.str());
				pool_.Destroy(handle);
				return Handle<Skymap>::null();
			}

			format = static_cast<DXGI_FORMAT>(header.format_);
			width = header.width_;
			height = header.height_;
			rowPitch = header.rowPitch_;
			slicePitch = header.dataSize_;
			pixels = cachePixels.data();
		}
		else
		{
			HRESULT loadResult = DirectX::LoadFromHDRFile(filePath.w_str().c_str(), nullptr, image);
			if (FAILED(loadResult))
			{
				SC_LOG_ERROR("スカイマップ HDR のロードに失敗しました: {}", filePath.str());
				pool_.Destroy(handle);
				return Handle<Skymap>::null();
			}

			const DirectX::TexMetadata& metadata = image.GetMetadata();
			const DirectX::Image* sourceImage = image.GetImage(0, 0, 0);
			if (!sourceImage)
			{
				SC_LOG_ERROR("スカイマップ equirect の画像取得に失敗しました: {}", filePath.str());
				pool_.Destroy(handle);
				return Handle<Skymap>::null();
			}

			format = metadata.format;
			width = static_cast<Uint>(metadata.width);
			height = static_cast<Uint>(metadata.height);
			rowPitch = static_cast<Uint>(sourceImage->rowPitch);
			slicePitch = static_cast<Uint>(sourceImage->slicePitch);
			pixels = sourceImage->pixels;
		}

		/// [EN] Create + upload manually rather than via DirectXTex's D3D12
		///      helpers (CreateTextureEx / PrepareUpload): the prebuilt
		///      DirectXTex.lib omits the D3D12 module. An equirect source is a
		///      single 2D image (1 mip, 1 slice), so one subresource suffices.
		/// [JP] DirectXTex の D3D12 ヘルパ（CreateTextureEx / PrepareUpload）は
		///      配置済み prebuilt lib に含まれないため、手動で生成＋アップロード
		///      する。equirect ソースは 2D 単一画像（1 ミップ 1 面）なのでサブ
		///      リソースは 1 つで足りる。
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = static_cast<Uint64>(width);
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = format;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&skymap->resource_));
		if (FAILED(hr) || !skymap->resource_)
		{
			SC_LOG_ERROR("スカイマップ equirect テクスチャの生成に失敗しました: {}", filePath.str());
			pool_.Destroy(handle);
			return Handle<Skymap>::null();
		}
#ifdef _DEBUG
		skymap->resource_->SetName(L"Skymap_Equirect");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(skymap->resource_.Get());
#endif

		D3D12_SUBRESOURCE_DATA subresource{};
		subresource.pData = pixels;
		subresource.RowPitch = static_cast<LONG_PTR>(rowPitch);
		subresource.SlicePitch = static_cast<LONG_PTR>(rowPitch) * static_cast<LONG_PTR>(height);

		DirectX::ResourceUploadBatch resourceUpload(device);
		resourceUpload.Begin();
		resourceUpload.Upload(skymap->resource_.Get(), 0, &subresource, 1);
		resourceUpload.Transition(skymap->resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		std::future<void> uploadFinished;
		{
			auto queueLock = cmdQueue->AcquireLock();
			uploadFinished = resourceUpload.End(cmdQueue->GetCommandQueue());
		}
		uploadFinished.wait();

		skymap->shaderResourceViewIndex_ = heap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = format;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(skymap->resource_.Get(), &shaderResourceViewDesc, heap->CPUHandle(skymap->shaderResourceViewIndex_));

		if (!fromCache)
		{
			/// [EN] Bake the decoded pixels to the sibling ".skymap" cache
			///      (best-effort) so the skymap can later be shipped / loaded
			///      standalone without the source HDR.
			/// [JP] デコード済みピクセルを隣の ".skymap" キャッシュへ焼く（ベスト
			///      エフォート）。後でソース HDR 無しでも単体で配布/ロードできるように。
			std::filesystem::path cacheFsPath(filePath.str());
			cacheFsPath.replace_extension(".skymap");

			SkymapCacheHeader outHeader{};
			std::memcpy(outHeader.magic_, skymapCacheMagic_, sizeof(outHeader.magic_));
			outHeader.version_ = skymapCacheVersion_;
			outHeader.format_ = static_cast<Uint32>(format);
			outHeader.width_ = static_cast<Uint32>(width);
			outHeader.height_ = static_cast<Uint32>(height);
			outHeader.rowPitch_ = static_cast<Uint32>(rowPitch);
			outHeader.dataSize_ = static_cast<Uint32>(slicePitch);
			WriteSkymapCache(String(cacheFsPath.string()), outHeader, pixels);
		}

		return handle;
	}

	Skymap* SkymapLoader::Get(const Handle<Skymap>& handle)
	{
		return pool_.Get(handle);
	}

	void SkymapLoader::Clear(Handle<Skymap>& handle, BindlessHeap* heap)noexcept
	{
		Skymap* skymap = pool_.Get(handle);
		if (skymap && heap)
		{
			if (skymap->shaderResourceViewIndex_ != SC_INVALID)
			{
				heap->FreeIndex(skymap->shaderResourceViewIndex_);
				skymap->shaderResourceViewIndex_ = SC_INVALID;
			}

			/// [EN] pool_.Destroy below runs the Skymap's destructor right here,
			///      so the equirect texture must be handed to the deferred ring
			///      first - the frames still in flight are sampling it.
			/// [JP] 下の pool_.Destroy はこの場で Skymap のデストラクタを走らせる
			///      ため、先に equirect テクスチャを遅延回収リングへ渡す必要が
			///      ある — インフライトのフレームがまだサンプリングしている。
			heap->DeferRelease(skymap->resource_);
			skymap->resource_.Reset();
		}

		pool_.Destroy(handle);
	}
}
