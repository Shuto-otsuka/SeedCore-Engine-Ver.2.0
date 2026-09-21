#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/BootConfig.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Buffer/FrameBuffer.h>
#include <GraphicsEngine/Shape/Screen/BootScreen.h>

namespace SeedCore
{
	class BindlessHeap;
	class D3D12CommandQueue;
	class D3D12CommandList;

	class SEEDCORE_API BootScreenRenderer :public NonCopyable
	{
	public:
		BootScreenRenderer() = default;
		~BootScreenRenderer() = default;

		void Create(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap, DescriptorHeap* imguiHeap, Uint32 width, Uint32 height);

		void Resize(Uint32 width, Uint32 height);

		void LoadImages(const BootConfig& config);

		void Render(D3D12CommandList* cmdList, const BootConfig& config, Float progress, Float time);

		[[nodiscard]] Bool Created()const;

		[[nodiscard]] Uint32 Width()const;

		[[nodiscard]] Uint32 Height()const;

		[[nodiscard]] Float BarAspect()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE ImGuiGPUHandle()const;

	private:
		void RegisterImGuiShaderResourceView();

	private:
		BootScreen bootScreen_;

		DescriptorHeap renderTargetViewHeap_;
		ResourcePtr<FrameBuffer> frameBuffer_;

		ID3D12Device* device_ = nullptr;
		BindlessHeap* bindlessHeap_ = nullptr;
		DescriptorHeap* imguiHeap_ = nullptr;

		Uint32 imguiShaderResourceViewIndex_ = 0;

		Uint32 width_ = 0;
		Uint32 height_ = 0;
	};
}
