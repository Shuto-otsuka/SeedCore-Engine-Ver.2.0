#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	struct RootAddresses
	{
		D3D12_GPU_VIRTUAL_ADDRESS shaderResource_ = 0;
		D3D12_GPU_VIRTUAL_ADDRESS unorderedAccess_ = 0;
		D3D12_GPU_VIRTUAL_ADDRESS constant_ = 0;
	};

	class RootSignature
	{
	public:
		RootSignature() = default;
		~RootSignature() = default;

		Handle<RootSignature> GetOrCreate(ID3D12Device* device);

		RootSignature* Get(const Handle<RootSignature>& handle);

		[[nodiscard]] ID3D12RootSignature* Get()const noexcept;

		static void ClearCache();

		static void BindCompute(ID3D12GraphicsCommandList* cmd, const RootAddresses& addresses);

		static void BindGraphics(ID3D12GraphicsCommandList* cmd, const RootAddresses& addresses);

	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	};
}