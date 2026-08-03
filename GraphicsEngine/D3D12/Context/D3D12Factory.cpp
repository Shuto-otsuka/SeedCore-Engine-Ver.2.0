#include <GraphicsEngine/D3D12/Context/D3D12Factory.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	Bool D3D12Factory::Create()
	{
		HRESULT hr{ S_OK };

#ifdef _DEBUG
		hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory_));
		SC_HR_CHECK(hr, "DXGIFactoryの生成に失敗しました");
#else
		hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory_));
		SC_HR_CHECK(hr, "DXGIFactoryの生成に失敗しました");
#endif

		return SUCCEEDED(hr);
	}

	IDXGIFactory7* D3D12Factory::Get()const
	{
		return factory_.Get();
	}
}