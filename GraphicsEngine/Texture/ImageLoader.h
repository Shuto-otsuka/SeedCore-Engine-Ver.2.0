#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Texture/Texture.h>

namespace SeedCore
{
	class TextureLoader;
	class BindlessHeap;

	class ImageLoader :public NonCopyable
	{
	public:
		ImageLoader() = default;
		~ImageLoader() = default;

		Handle<Texture> Load(TextureLoader& textureLoader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, String filePath);

		Texture* Resolve(TextureLoader& textureLoader, BindlessHeap* heap, const Handle<Texture>& handle, Uint64 frame);

		Texture* Get(const Handle<Texture>& handle);

		void Clear(Handle<Texture>& handle, BindlessHeap* heap)noexcept;

		void EvictBudget(BindlessHeap* heap, Uint64 currentFrame);

	private:
		StablePool<Texture> pool_;
		DynamicArray<Handle<Texture>> loadedHandles_;

		ID3D12Device* device_ = nullptr;
		ID3D12CommandQueue* cmdQueue_ = nullptr;

		Uint64 totalResidentBytes_ = 0;
		Uint64 budgetBytes_ = 128ull * 1024 * 1024;
		static constexpr Uint64 evictAgeFrames_ = 8;
	};
}
