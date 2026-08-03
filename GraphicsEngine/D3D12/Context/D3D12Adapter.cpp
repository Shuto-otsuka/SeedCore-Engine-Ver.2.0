#include <GraphicsEngine/D3D12/Context/D3D12Adapter.h>

namespace SeedCore
{
	Bool D3D12Adapter::Create(IDXGIFactory7* factory)
	{
		HRESULT hr{ S_OK };

		Microsoft::WRL::ComPtr<IDXGIAdapter4> bestAdapter = nullptr;
		Size maxVRAM = 0;
		Uint index = 0;

		while (true)
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter4> currentAdapter = nullptr;
			hr = factory->EnumAdapterByGpuPreference(index++, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&currentAdapter));

			if (hr == DXGI_ERROR_NOT_FOUND)
			{
				break;
			}

			DXGI_ADAPTER_DESC3 desc{};
			currentAdapter->GetDesc3(&desc);

			if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
			{
				if (desc.DedicatedVideoMemory > maxVRAM)
				{
					maxVRAM = desc.DedicatedVideoMemory;
					bestAdapter = currentAdapter;
				}
			}
		}

		if (!bestAdapter)
		{
			return false;
		}

		adapter_ = bestAdapter;
		return true;
	}

	IDXGIAdapter4* D3D12Adapter::Get()const
	{
		return adapter_.Get();
	}
}