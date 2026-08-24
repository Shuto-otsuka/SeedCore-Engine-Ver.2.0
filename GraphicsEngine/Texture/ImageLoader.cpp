#include <GraphicsEngine/Texture/ImageLoader.h>
#include <GraphicsEngine/Resource/TextureLoader.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	Handle<Texture> ImageLoader::Load(TextureLoader& textureLoader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, String filePath)
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

		textureLoader.CreateTexture(device, cmdQueue, heap->Heap(), filePath, texture->resource_, texture->textureIndex_);

		if (texture->resource_)
		{
			D3D12_RESOURCE_DESC desc = texture->resource_->GetDesc();
			texture->sizeBytes_ = device->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;
			totalResidentBytes_ += texture->sizeBytes_;
		}

		loadedHandles_.push_back(handle);

		return handle;
	}

	Texture* ImageLoader::Resolve(TextureLoader& textureLoader, BindlessHeap* heap, const Handle<Texture>& handle, Uint64 frame)
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
			textureLoader.CreateTexture(device_, cmdQueue_, heap->Heap(), texture->filePath_, texture->resource_, texture->textureIndex_);
			if (texture->resource_)
			{
				D3D12_RESOURCE_DESC desc = texture->resource_->GetDesc();
				texture->sizeBytes_ = device_->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;
				totalResidentBytes_ += texture->sizeBytes_;
			}
		}

		return texture;
	}

	Texture* ImageLoader::Get(const Handle<Texture>& handle)
	{
		return pool_.Get(handle);
	}

	void ImageLoader::Clear(Handle<Texture>& handle, BindlessHeap* heap)noexcept
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
			auto found = std::find_if(loadedHandles_.begin(), loadedHandles_.end(), [&handle](const Handle<Texture>& loadedHandle) { return loadedHandle == handle; });
			if (found != loadedHandles_.end())
			{
				loadedHandles_.erase(found);
			}
		}
		pool_.Destroy(handle);
	}

	void ImageLoader::EvictBudget(BindlessHeap* heap, Uint64 currentFrame)
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

		std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
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
}
