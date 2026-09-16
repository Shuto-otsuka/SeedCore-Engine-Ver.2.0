#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Texture/Texture.h>

namespace SeedCore
{
	class BindlessHeap;
	class D3D12CommandQueue;

	class SEEDCORE_API TextureLoader :public NonCopyable
	{
	public:
		TextureLoader() = default;
		~TextureLoader() = default;

		Handle<Texture> Load(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, String filePath);

		Texture* Get(const Handle<Texture>& handle);

		void Clear(Handle<Texture>& handle, BindlessHeap* heap)noexcept;

		Texture* Resolve(BindlessHeap* heap, const Handle<Texture>& handle, Uint64 frame);

		void EvictBudget(BindlessHeap* heap, Uint64 currentFrame);

	public:
		static void CreateTexture(in ID3D12Device* device, in D3D12CommandQueue* cmdQueue, in ID3D12DescriptorHeap* heap, in String filePath, inout Microsoft::WRL::ComPtr<ID3D12Resource>& resource, in Uint textureIndex);

	private:
		StablePool<Texture> pool_;
		DynamicArray<Handle<Texture>> loadedHandles_;

		ID3D12Device* device_ = nullptr;
		D3D12CommandQueue* cmdQueue_ = nullptr;

		Uint64 totalResidentBytes_ = 0;
		Uint64 budgetBytes_ = 128ull * 1024 * 1024;
		static constexpr Uint64 evictAgeFrames_ = 8;
	};
}
