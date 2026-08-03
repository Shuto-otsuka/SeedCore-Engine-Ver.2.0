#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/D3D12Common.h>

namespace SeedCore
{
	class D3D12Factory :public NonTransferable
	{
	public:
		Bool Create();

		IDXGIFactory7* Get()const;

	private:
		Microsoft::WRL::ComPtr<IDXGIFactory7> factory_;
	};
}