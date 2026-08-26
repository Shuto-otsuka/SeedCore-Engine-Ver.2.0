#include <GraphicsEngine/Resource/TextureLoader.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Error.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>
#include <FoundationEngine/File/FileUtility.h>

namespace SeedCore
{
	void TextureLoader::CreateTexture(in ID3D12Device* device, in D3D12CommandQueue* cmdQueue, in ID3D12DescriptorHeap* heap, in String filePath, inout Microsoft::WRL::ComPtr<ID3D12Resource>& resource, in Uint textureIndex)
	{
		HRESULT hr{ S_OK };

		DirectX::ResourceUploadBatch resourceUpload(device);
		resourceUpload.Begin();

		std::string extension = std::filesystem::path(filePath.str()).extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](Uchar c) { return static_cast<Char>(std::tolower(c)); });

		if (extension == ".icon" || extension == ".logo" || extension == ".texture")
		{
			BinaryInputArchive archive;
			if (archive.Read(filePath))
			{
				DynamicArray<Byte> data;
				archive.TryField("data", data);
				if (!data.empty())
				{
					hr = DirectX::CreateDDSTextureFromMemory(device, resourceUpload, reinterpret_cast<const Uint8*>(data.data()), data.size(), &resource);

					if (FAILED(hr))
					{
						hr = DirectX::CreateWICTextureFromMemory(device, resourceUpload, reinterpret_cast<const Uint8*>(data.data()), data.size(), &resource);
					}
				}
			}
		}
		else
		{
			/// [EN] Source-preferred, mirroring ModelLoader's ".crister" cache
			///      handling: only trust the sibling ".texture" cache when it's
			///      newer than the source image. Otherwise (re)load from source
			///      and (re)bake the encrypted cache.
			/// [JP] ソース優先、ModelLoaderの".crister"キャッシュ扱いと同じ構図:
			///      隣の".texture"キャッシュは、ソース画像より新しい時だけ信用
			///      する。それ以外はソースから(再)ロードし、暗号化キャッシュを
			///      (再)ベイクする。
			std::filesystem::path sourceFsPath(filePath.str());
			std::filesystem::path cacheFsPath = sourceFsPath;
			cacheFsPath.replace_extension(".texture");

			Bool loadedFromCache = false;
			if (std::filesystem::exists(cacheFsPath) && std::filesystem::exists(sourceFsPath) && std::filesystem::last_write_time(cacheFsPath) >= std::filesystem::last_write_time(sourceFsPath))
			{
				BinaryInputArchive archive;
				if (archive.Read(String(cacheFsPath.string())))
				{
					DynamicArray<Byte> data;
					archive.TryField("data", data);
					if (!data.empty())
					{
						hr = DirectX::CreateDDSTextureFromMemory(device, resourceUpload, reinterpret_cast<const Uint8*>(data.data()), data.size(), &resource);

						if (FAILED(hr))
						{
							hr = DirectX::CreateWICTextureFromMemory(device, resourceUpload, reinterpret_cast<const Uint8*>(data.data()), data.size(), &resource);
						}

						loadedFromCache = SUCCEEDED(hr) && resource;
					}
				}
			}

			if (!loadedFromCache)
			{
				hr = DirectX::CreateDDSTextureFromFile(device, resourceUpload, filePath.w_str().c_str(), &resource);

				if (FAILED(hr))
				{
					hr = DirectX::CreateWICTextureFromFile(device, resourceUpload, filePath.w_str().c_str(), &resource, false);
				}

				if (SUCCEEDED(hr) && resource)
				{
					DynamicArray<Uint8> sourceBytes = FileUtility::LoadFileBinary(filePath);
					if (!sourceBytes.empty())
					{
						BinaryOutputArchive cacheArchive;
						cacheArchive.Field("data", sourceBytes);
						cacheArchive.Write(String(cacheFsPath.string()));
					}
				}
			}
		}

		if (FAILED(hr) || !resource)
		{
			/// [EN] Bail out instead of dereferencing a null resource below.
			/// [JP] 下で null リソースを参照しないよう、ここで中断する。
			SC_LOG_ERROR("テクスチャのロードに失敗しました: {}", filePath.str());

			/// [EN] Lock only around End() itself (ExecuteCommandLists/Signal,
			///      fast) - not the wait() below, which just blocks on a CPU
			///      event and never touches the queue, so holding the lock
			///      there would stall the main thread's own per-frame
			///      Signal()/Execute() for no reason.
			/// [JP] End() 自体(ExecuteCommandLists/Signal、高速)だけをロックする
			///      - 下の wait() は CPU イベントを待つだけでキューには一切
			///      触れないので、そこまでロックを持ったままだとメインスレッド
			///      の毎フレームの Signal()/Execute() を無意味に足止めする。
			std::future<void> uploadAborted;
			{
				auto queueLock = cmdQueue->AcquireLock();
				uploadAborted = resourceUpload.End(cmdQueue->GetCommandQueue());
			}
			uploadAborted.wait();
			return;
		}

		std::future<void> uploadFinished;
		{
			auto queueLock = cmdQueue->AcquireLock();
			uploadFinished = resourceUpload.End(cmdQueue->GetCommandQueue());
		}
		uploadFinished.wait();

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = resource->GetDesc().Format;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
		Uint64 descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuHandle.ptr += textureIndex * descriptorSize;

		device->CreateShaderResourceView(resource.Get(), &shaderResourceViewDesc, cpuHandle);
	}
}
