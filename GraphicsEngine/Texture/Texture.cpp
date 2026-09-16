#include <GraphicsEngine/Texture/Texture.h>

namespace SeedCore
{
	void Texture::Pin()
	{
		pinned_ = true;
	}

	Handle<Texture> Texture::GetHandle()const
	{
		return handle_;
	}

	Uint Texture::ShaderResourceViewIndex()const
	{
		return textureIndex_;
	}

	ID3D12Resource* Texture::Resource()const
	{
		return resource_.Get();
	}

	const String& Texture::FilePath()const
	{
		return filePath_;
	}
}
