#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	struct Texture
	{
		Handle<Texture> handle_;
		Uint textureIndex_;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;

		String filePath_;
		Uint64 sizeBytes_ = 0;
		Uint64 lastUsedFrame_ = 0;
		Bool pinned_ = false;
	};
}
