#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/ArtMap.h>

namespace SeedCore
{
	class ComputeShader
	{
	public:
		ComputeShader() = default;
		~ComputeShader() = default;

		void SetBlob(Microsoft::WRL::ComPtr<IDxcBlob> blob, Microsoft::WRL::ComPtr<IDxcBlob> reflectionBlob);

		[[nodiscard]] D3D12_SHADER_BYTECODE Bytecode()const noexcept;

		[[nodiscard]] IDxcBlob* Blob()const noexcept;

		[[nodiscard]] IDxcBlob* ReflectionBlob()const noexcept;

	private:
		Microsoft::WRL::ComPtr<IDxcBlob> blob_;
		Microsoft::WRL::ComPtr<IDxcBlob> reflectionBlob_;
	};
}