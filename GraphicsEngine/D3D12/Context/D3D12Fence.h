#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/D3D12Common.h>

namespace SeedCore
{
	class D3D12Fence :public NonTransferable
	{
	public:
		Bool Create(ID3D12Device* device);

		Uint64 Next();

		ID3D12Fence* Get()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

		Uint64 fenceValue_{ 0 };
	};
}