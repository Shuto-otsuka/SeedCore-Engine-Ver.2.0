#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class D3D12CommandQueue;

	class SEEDCORE_API TextureLoader :public NonCopyable
	{
	public:
		TextureLoader() = default;
		~TextureLoader() = default;

		void CreateTexture(in ID3D12Device* device, in D3D12CommandQueue* cmdQueue, in ID3D12DescriptorHeap* heap, in String filePath, inout Microsoft::WRL::ComPtr<ID3D12Resource>& resource, in Uint textureIndex);
	};
}