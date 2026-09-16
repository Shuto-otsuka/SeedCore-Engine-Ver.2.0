#include <GraphicsEngine/Texture/TextureLoader.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Error.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>
#include <FoundationEngine/File/FileUtility.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	Handle<Texture> TextureLoader::Load(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, String filePath)
	{
		Handle<Texture> handle = pool_.Create();
		Texture* texture = pool_.Get(handle);
		if (!texture)
		{
			return Handle<Texture>::null();
		}

		device_ = device;
		cmdQueue_ = cmdQueue;

		texture->handle_ = handle;
		texture->textureIndex_ = heap->AllocateIndex();
		texture->filePath_ = filePath;

		CreateTexture(device, cmdQueue, heap->Heap(), filePath, texture->resource_, texture->textureIndex_);

		if (texture->resource_)
		{
			D3D12_RESOURCE_DESC desc = texture->resource_->GetDesc();
			texture->sizeBytes_ = device->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;
			totalResidentBytes_ += texture->sizeBytes_;
		}

		loadedHandles_.push_back(handle);

		return handle;
	}

	Texture* TextureLoader::Get(const Handle<Texture>& handle)
	{
		return pool_.Get(handle);
	}

	void TextureLoader::Clear(Handle<Texture>& handle, BindlessHeap* heap)noexcept
	{
		Texture* texture = pool_.Get(handle);
		if (texture)
		{
			if (texture->resource_)
			{
				heap->FreeIndex(texture->textureIndex_);
				/// [EN] pool_.Destroy below runs the Texture's destructor right
				///      here, so the resource must be handed to the deferred
				///      ring first - the frames still in flight are sampling it.
				/// [JP] 下の pool_.Destroy はこの場で Texture のデストラクタを
				///      走らせるため、先にリソースを遅延回収リングへ渡す必要が
				///      ある — インフライトのフレームがまだサンプリングしている。
				heap->DeferRelease(texture->resource_);
				totalResidentBytes_ -= texture->sizeBytes_;
			}
			auto found = std::ranges::find_if(loadedHandles_, [handle](const auto& candidate){ return candidate == handle; });
			if (found != loadedHandles_.end())
			{
				loadedHandles_.erase(found);
			}
		}
		pool_.Destroy(handle);
	}

	Texture* TextureLoader::Resolve(BindlessHeap* heap, const Handle<Texture>& handle, Uint64 frame)
	{
		Texture* texture = pool_.Get(handle);
		if (!texture)
		{
			return nullptr;
		}

		texture->lastUsedFrame_ = frame;

		if (!texture->resource_ && device_ && cmdQueue_)
		{
			texture->textureIndex_ = heap->AllocateIndex();
			CreateTexture(device_, cmdQueue_, heap->Heap(), texture->filePath_, texture->resource_, texture->textureIndex_);
			if (texture->resource_)
			{
				D3D12_RESOURCE_DESC desc = texture->resource_->GetDesc();
				texture->sizeBytes_ = device_->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;
				totalResidentBytes_ += texture->sizeBytes_;
			}
		}

		return texture;
	}

	void TextureLoader::EvictBudget(BindlessHeap* heap, Uint64 currentFrame)
	{
		if (totalResidentBytes_ <= budgetBytes_)
		{
			return;
		}

		struct Candidate
		{
			Handle<Texture> handle_;
			Uint64 lastUsedFrame_;
		};
		DynamicArray<Candidate> candidates;
		for (const Handle<Texture>& handle : loadedHandles_)
		{
			Texture* texture = pool_.Get(handle);
			if (texture && texture->resource_ && !texture->pinned_ && texture->lastUsedFrame_ + evictAgeFrames_ <= currentFrame)
			{
				candidates.push_back({ handle, texture->lastUsedFrame_ });
			}
		}

		std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b)
		{
			return a.lastUsedFrame_ < b.lastUsedFrame_;
		});

		for (const Candidate& candidate : candidates)
		{
			if (totalResidentBytes_ <= budgetBytes_)
			{
				break;
			}

			Texture* texture = pool_.Get(candidate.handle_);
			if (!texture)
			{
				continue;
			}

			heap->FreeIndex(texture->textureIndex_);
			texture->textureIndex_ = 0xFFFFFFFF;
			heap->DeferRelease(texture->resource_);
			texture->resource_.Reset();
			totalResidentBytes_ -= texture->sizeBytes_;
			texture->sizeBytes_ = 0;
		}
	}

	void TextureLoader::CreateTexture(in ID3D12Device* device, in D3D12CommandQueue* cmdQueue, in ID3D12DescriptorHeap* heap, in String filePath, inout Microsoft::WRL::ComPtr<ID3D12Resource>& resource, in Uint textureIndex)
	{
		HRESULT hr{ S_OK };

		DirectX::ResourceUploadBatch resourceUpload(device);
		resourceUpload.Begin();

		std::string extension = std::filesystem::path(filePath.str()).extension().string();
		std::ranges::transform(extension, extension.begin(), [](Uchar c) { return static_cast<Char>(std::tolower(c)); });

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
