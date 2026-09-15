#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class FadeScreen
	{
	public:
		FadeScreen() = default;
		~FadeScreen() = default;

		void Initialize(ID3D12Device* device);

		void Finalize();

		void Draw(ID3D12GraphicsCommandList6* cmdList, Float alpha, Float screenWidth, Float screenHeight);

	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

		Bool initialized_ = false;
	};
}
