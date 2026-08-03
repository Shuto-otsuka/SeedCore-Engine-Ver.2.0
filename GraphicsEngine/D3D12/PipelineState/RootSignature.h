#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{

	class RootSignature
	{
	public:
		RootSignature() = default;
		~RootSignature() = default;

		Handle<RootSignature> GetOrCreate(ID3D12Device* device);

		RootSignature* Get(const Handle<RootSignature>& handle);

		[[nodiscard]] ID3D12RootSignature* Get()const noexcept;

		static void ClearCache();

	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	};
}