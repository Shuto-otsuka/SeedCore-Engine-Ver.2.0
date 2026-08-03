#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class TextureLoader;
	class BindlessHeap;
	class D3D12CommandQueue;

	class SplashScreen
	{
	public:
		SplashScreen() = default;
		~SplashScreen() = default;

		void Initialize(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap);

		void Draw(ID3D12GraphicsCommandList6* cmdList, ID3D12Resource* backBuffer, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, Float screenWidth, Float screenHeight, Bool loadComplete);

		[[nodiscard]] Bool IsFinished()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> dayResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> nightResource_;

		Uint dayTextureIndex_ = 0;
		Uint nightTextureIndex_ = 0;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

		Float minDuration_ = 3.0f;
		Float fadeInTime_ = 0.5f;
		Float fadeOutTime_ = 0.5f;

		Bool finished_ = false;
		Bool initialized_ = false;
		Bool started_ = false;

		BindlessHeap* bindlessHeap_ = nullptr;
		D3D12CommandQueue* cmdQueue_ = nullptr;

		std::chrono::steady_clock::time_point startTime_;
	};
}
