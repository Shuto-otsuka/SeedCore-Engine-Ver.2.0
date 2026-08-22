#include <GraphicsEngine/Sky/Skymap.h>
#include <GraphicsEngine/Sky/SkymapCache.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	Bool Skymap::LoadEquirect(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, const String& filePath)
	{
		DirectX::ScratchImage image;
		HRESULT hr = DirectX::LoadFromHDRFile(filePath.w_str().c_str(), nullptr, image);
		if (FAILED(hr))
		{
			SC_LOG_ERROR("スカイマップ HDR のロードに失敗しました: {}", filePath.str());
			return false;
		}

		const DirectX::TexMetadata& metadata = image.GetMetadata();
		const DirectX::Image* sourceImage = image.GetImage(0, 0, 0);
		if (!sourceImage)
		{
			SC_LOG_ERROR("スカイマップ equirect の画像取得に失敗しました: {}", filePath.str());
			return false;
		}

		if (!CreateEquirectTexture(device, cmdQueue, heap, metadata.format, static_cast<Uint>(metadata.width), static_cast<Uint>(metadata.height), static_cast<Uint>(sourceImage->rowPitch), sourceImage->pixels))
		{
			SC_LOG_ERROR("スカイマップ equirect テクスチャの生成に失敗しました: {}", filePath.str());
			return false;
		}

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
		outHeader.format_ = static_cast<Uint32>(metadata.format);
		outHeader.width_ = static_cast<Uint32>(metadata.width);
		outHeader.height_ = static_cast<Uint32>(metadata.height);
		outHeader.rowPitch_ = static_cast<Uint32>(sourceImage->rowPitch);
		outHeader.dataSize_ = static_cast<Uint32>(sourceImage->slicePitch);
		WriteSkymapCache(String(cacheFsPath.string()), outHeader, sourceImage->pixels);

		return true;
	}

	Bool Skymap::LoadSkymapCache(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, const String& filePath)
	{
		SkymapCacheHeader header{};
		DynamicArray<Uint8> pixels;
		if (!ReadSkymapCache(filePath, header, pixels))
		{
			SC_LOG_ERROR("スカイマップキャッシュのロードに失敗しました: {}", filePath.str());
			return false;
		}

		if (!CreateEquirectTexture(device, cmdQueue, heap, static_cast<DXGI_FORMAT>(header.format_), header.width_, header.height_, header.rowPitch_, pixels.data()))
		{
			SC_LOG_ERROR("スカイマップキャッシュのテクスチャ生成に失敗しました: {}", filePath.str());
			return false;
		}

		return true;
	}

	Bool Skymap::CreateEquirectTexture(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, DXGI_FORMAT format, Uint width, Uint height, Uint rowPitch, const void* pixels)
	{
		HRESULT hr{ S_OK };

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

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&equirectResource_));
		if (FAILED(hr) || !equirectResource_)
		{
			return false;
		}

		D3D12_SUBRESOURCE_DATA subresource{};
		subresource.pData = pixels;
		subresource.RowPitch = static_cast<LONG_PTR>(rowPitch);
		subresource.SlicePitch = static_cast<LONG_PTR>(rowPitch) * static_cast<LONG_PTR>(height);

		DirectX::ResourceUploadBatch resourceUpload(device);
		resourceUpload.Begin();
		resourceUpload.Upload(equirectResource_.Get(), 0, &subresource, 1);
		resourceUpload.Transition(equirectResource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		auto uploadFinished = resourceUpload.End(cmdQueue);
		uploadFinished.wait();

		equirectShaderResourceViewIndex_ = heap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = format;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(equirectResource_.Get(), &shaderResourceViewDesc, heap->CPUHandle(equirectShaderResourceViewIndex_));

		return true;
	}

	void Skymap::Release(BindlessHeap* heap)noexcept
	{
		if (!heap)
		{
			return;
		}
		if (equirectShaderResourceViewIndex_ != invalidIndex_)
		{
			heap->FreeIndex(equirectShaderResourceViewIndex_);
			equirectShaderResourceViewIndex_ = invalidIndex_;
		}

		/// [EN] SkymapLoader::Clear calls this and then destroys the Skymap
		///      outright, so the equirect texture the in-flight frames are still
		///      sampling has to outlive both.
		/// [JP] SkymapLoader::Clear はこれを呼んだ直後に Skymap 自体を破棄する
		///      ため、インフライトのフレームがまだサンプリングしている equirect
		///      テクスチャは両者より長く生存させる必要がある。
		heap->DeferRelease(equirectResource_);
		equirectResource_.Reset();
	}

	Uint Skymap::EquirectShaderResourceViewIndex()const
	{
		return equirectShaderResourceViewIndex_;
	}

	Bool Skymap::Valid()const
	{
		return equirectShaderResourceViewIndex_ != invalidIndex_;
	}

	Handle<Skymap> Skymap::GetHandle()const
	{
		return handle_;
	}

	void Skymap::SetHandle(const Handle<Skymap>& handle)
	{
		handle_ = handle;
	}
}
