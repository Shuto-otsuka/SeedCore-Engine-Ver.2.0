#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/BootConfig.h>

namespace SeedCore
{
	class BindlessHeap;
	class D3D12CommandQueue;

	struct BootScreenConstantBuffer
	{
		Vector2 screenSize_;
		Float alpha_;
		Float progress_;

		Float time_;
		Float backgroundAspect_;
		Uint fillMethod_;
		Uint fillOrigin_;

		Uint clockwise_;
		Uint wave_;
		Uint useBackgroundImage_;
		Uint useFrame_;

		Vector4 barRect_;

		Vector4 backgroundColor_;
		Vector4 backgroundTint_;
		Vector4 barTint_;
		Vector4 frameTint_;
	};

	class SEEDCORE_API BootScreen
	{
	public:
		BootScreen() = default;
		~BootScreen() = default;

		void Initialize(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap, DXGI_FORMAT renderTargetFormat);

		void Finalize();

		void LoadImages(const BootConfig& config);

		void Draw(ID3D12GraphicsCommandList6* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, const BootConfig& config, Float screenWidth, Float screenHeight, Float progress, Float time, Float alpha);

		[[nodiscard]] Bool Initialized()const;

		[[nodiscard]] Float BarAspect()const;

		[[nodiscard]] static Vector4 BarRect(const BootConfig& config, Float screenWidth, Float screenHeight, Float barAspect);

	private:
		void ReplaceImage(const DynamicArray<Byte>& image, const Char* defaultName, Uint& textureIndex, Microsoft::WRL::ComPtr<ID3D12Resource>& resource);

		void ReleaseImage(Uint& textureIndex, Microsoft::WRL::ComPtr<ID3D12Resource>& resource);

		[[nodiscard]] static Float ImageAspect(const Microsoft::WRL::ComPtr<ID3D12Resource>& resource);

	private:
		static constexpr Uint invalidIndex_ = 0xFFFFFFFF;

		Microsoft::WRL::ComPtr<ID3D12Resource> backgroundResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> barResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> frameResource_;

		Uint backgroundTextureIndex_ = invalidIndex_;
		Uint barTextureIndex_ = invalidIndex_;
		Uint frameTextureIndex_ = invalidIndex_;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

		ID3D12Device* device_ = nullptr;
		D3D12CommandQueue* cmdQueue_ = nullptr;
		BindlessHeap* bindlessHeap_ = nullptr;

		Bool initialized_ = false;
	};
}
