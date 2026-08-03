#include <GraphicsEngine/D3D12/Context/D3D12Device.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	Bool D3D12Device::Create(IDXGIAdapter4* adapter)
	{
		HRESULT hr{ S_OK };

		static const D3D_FEATURE_LEVEL levels[] =
		{
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

		for (auto level : levels)
		{
			hr = D3D12CreateDevice(adapter, level, IID_PPV_ARGS(&device_));

			if (hr == S_OK)
			{
				featureLevel_ = level;
				QueryRaytracingSupport();
				return true;
			}
		}
		SC_HR_CHECK(hr, "Direct3D 12 デバイスの作成に失敗しました。対応する機能レベルが見つかりません。");

		return false;
	}

	void D3D12Device::QueryRaytracingSupport()
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
		HRESULT hr = device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
		if (FAILED(hr))
		{
			raytracingTier_ = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
			SC_LOG_WARNING("DXR対応状況を照会できませんでした。レイトレーシングは無効として扱います。");
			return;
		}

		raytracingTier_ = options5.RaytracingTier;

		if (raytracingTier_ >= D3D12_RAYTRACING_TIER_1_1)
		{
			SC_LOG_NOTICE("DXR Tier 1.1 対応: インラインレイトレーシング(RayQuery)が利用可能です。");
		}
		else if (raytracingTier_ >= D3D12_RAYTRACING_TIER_1_0)
		{
			SC_LOG_WARNING("DXR Tier 1.0 のみ対応: DispatchRaysは可能ですがRayQueryは利用できません。");
		}
		else
		{
			SC_LOG_WARNING("このGPUはDXR非対応です。レイトレーシング機能は無効になります。");
		}
	}

	ID3D12Device5* D3D12Device::Get()const
	{
		return device_.Get();
	}

	D3D12_RAYTRACING_TIER D3D12Device::RaytracingTier()const noexcept
	{
		return raytracingTier_;
	}

	Bool D3D12Device::RaytracingSupported()const noexcept
	{
		return raytracingTier_ >= D3D12_RAYTRACING_TIER_1_0;
	}

	Bool D3D12Device::InlineRaytracingSupported()const noexcept
	{
		return raytracingTier_ >= D3D12_RAYTRACING_TIER_1_1;
	}
}