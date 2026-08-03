#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/D3D12Common.h>

namespace SeedCore
{
	class SEEDCORE_API D3D12Adapter :public NonTransferable
	{
	public:
		Bool Create(IDXGIFactory7* factory);

		IDXGIAdapter4* Get()const;

	private:
		Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
	};
}