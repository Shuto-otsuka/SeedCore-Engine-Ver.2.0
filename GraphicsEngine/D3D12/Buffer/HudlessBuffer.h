#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class D3D12CommandList;

	class HudlessBuffer
	{
	public:
		HudlessBuffer() = default;
		~HudlessBuffer() = default;

		void Create(ID3D12Device* device, Uint32 width, Uint32 height);

		void Resize(ID3D12Device* device, Uint32 width, Uint32 height);

		void Capture(D3D12CommandList* cmdList, ID3D12Resource* source);

		[[nodiscard]] ID3D12Resource* ColorResource()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> colorResource_;

		D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;

		Uint32 width_ = 0;

		Uint32 height_ = 0;
	};
}
