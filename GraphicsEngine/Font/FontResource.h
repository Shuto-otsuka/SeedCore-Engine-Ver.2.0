#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <GraphicsEngine/Font/Font.h>

namespace SeedCore
{
	class BindlessHeap;

	class FontResource :public NonCopyable
	{
	public:
		FontResource() = default;
		~FontResource() = default;

		Font* Load(Uint32 assetID, const std::string& filePath, Float fontSize);

		Font* Find(Uint32 assetID)const;

		Bool Contains(Uint32 assetID)const;

		void Unload(Uint32 assetID);

		void Update(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap);

		void Clear();

	private:
		FlatMap<Uint32, ResourcePtr<Font>> fonts_;
	};
}
