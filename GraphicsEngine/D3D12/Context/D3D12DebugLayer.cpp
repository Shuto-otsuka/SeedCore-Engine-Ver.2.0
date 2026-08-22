#include <GraphicsEngine/D3D12/Context/D3D12DebugLayer.h>

namespace SeedCore
{
	void D3D12DebugLayer::Enable()
	{
#ifdef _DEBUG
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

#if DEEP_D3D12_DEBUG_MODE
			Microsoft::WRL::ComPtr<ID3D12Debug1> debugController1;
			if (SUCCEEDED(debugController.As(&debugController1)))
			{
				debugController1->SetEnableGPUBasedValidation(TRUE);
			}
#endif
		}
#endif

		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dredSettings;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings))))
		{
			dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			dredSettings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		}
	}

	void D3D12DebugLayer::Report()
	{
#ifdef _DEBUG
		Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		}
#endif
	}
}