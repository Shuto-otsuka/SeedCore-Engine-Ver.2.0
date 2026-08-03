#include <GraphicsEngine/Font/FontResource.h>
#include <GraphicsEngine/Font/FontManager.h>
#include <FoundationEngine/Resource/Gateway.h>

namespace SeedCore
{
	Font* FontResource::Load(Uint32 assetID, const std::string& filePath, Float fontSize)
	{
		if (Font* found = Find(assetID))
		{
			return found;
		}

		ResourcePtr<Font> font = MakePtr<Font>();
		if (!font->Initialize(Gateway::GetFontManager().GetLibrary(), filePath, fontSize))
		{
			font->Finalize();
			return nullptr;
		}

		Font* result = font.get();
		fonts_.insert({ assetID, std::move(font) });
		return result;
	}

	Font* FontResource::Find(Uint32 assetID)const
	{
		if (!fonts_.contains(assetID))
		{
			return nullptr;
		}
		return fonts_.at(assetID).get();
	}

	Bool FontResource::Contains(Uint32 assetID)const
	{
		return fonts_.contains(assetID);
	}

	void FontResource::Unload(Uint32 assetID)
	{
		if (!fonts_.contains(assetID))
		{
			return;
		}

		fonts_.at(assetID)->Finalize();
		fonts_.erase(assetID);
	}

	void FontResource::Update(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap)
	{
		for (auto& font : fonts_ | std::ranges::views::values)
		{
			font->Update();
			font->UploadAtlas(device, cmdQueue, bindlessHeap);
		}
	}

	void FontResource::Clear()
	{
		for (auto& font : fonts_ | std::ranges::views::values)
		{
			font->Finalize();
		}
		fonts_.clear();
	}
}
