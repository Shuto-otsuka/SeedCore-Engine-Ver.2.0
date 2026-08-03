#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/D3D12Common.h>

namespace SeedCore
{
	class D3D12Device :public NonTransferable
	{
	public:
		Bool Create(IDXGIAdapter4* adapter);

		ID3D12Device5* Get()const;

		[[nodiscard]] D3D12_RAYTRACING_TIER RaytracingTier()const noexcept;

		[[nodiscard]] Bool RaytracingSupported()const noexcept;

		[[nodiscard]] Bool InlineRaytracingSupported()const noexcept;

	private:
		void QueryRaytracingSupport();

		Microsoft::WRL::ComPtr<ID3D12Device5> device_;

		D3D_FEATURE_LEVEL featureLevel_;

		D3D12_RAYTRACING_TIER raytracingTier_ = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	};
}