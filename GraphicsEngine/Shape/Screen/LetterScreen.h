#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class BindlessHeap;

	class LetterScreen
	{
	public:
		LetterScreen() = default;
		~LetterScreen() = default;

		void Initialize(ID3D12Device* device, BindlessHeap* bindlessHeap);

		void Finalize();

		void Draw(ID3D12GraphicsCommandList6* cmdList, ID3D12Resource* sourceResource, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, Float screenWidth, Float screenHeight);

	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

		ID3D12Device* device_ = nullptr;

		BindlessHeap* bindlessHeap_ = nullptr;

		Uint sourceTextureIndex_ = 0;

		Bool initialized_ = false;
	};
}
