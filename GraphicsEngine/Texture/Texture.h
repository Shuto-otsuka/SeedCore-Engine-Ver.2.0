#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	class SEEDCORE_API Texture :public NonCopyable
	{
		friend class TextureLoader;

	public:
		Texture() = default;
		~Texture() = default;

		Texture(Texture&&)noexcept = default;
		Texture& operator=(Texture&&)noexcept = default;

	public:
		void Pin();

	public:
		[[nodiscard]] Uint ShaderResourceViewIndex()const;

		[[nodiscard]] ID3D12Resource* Resource()const;

		[[nodiscard]] const String& FilePath()const;

		[[nodiscard]] Handle<Texture> GetHandle()const;

	private:
		Handle<Texture> handle_;

		Uint textureIndex_ = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;

		String filePath_;

		Uint64 sizeBytes_ = 0;

		Uint64 lastUsedFrame_ = 0;

		Bool pinned_ = false;
	};
}
