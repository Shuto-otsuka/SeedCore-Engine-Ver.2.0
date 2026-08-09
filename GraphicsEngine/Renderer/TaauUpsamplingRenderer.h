#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/TAAU/TaauResolveShader.h>
#include <GraphicsEngine/DLSS/DlssBackgroundVelocityShader.h>
#include <GraphicsEngine/Raytracing/RaytracingView.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;

	class TaauUpsamplingRenderer
	{
	public:
		TaauUpsamplingRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~TaauUpsamplingRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 outputWidth, Uint32 outputHeight);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 outputWidth, Uint32 outputHeight);

		void PrepareView(RaytracingView view);

		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view, ID3D12Resource* velocityResource, Uint32 colorShaderResourceViewIndex, Uint32 depthShaderResourceViewIndex, Uint32 velocityShaderResourceViewIndex, Uint32 sourceWidth, Uint32 sourceHeight);

		[[nodiscard]] ID3D12Resource* OutputResource(RaytracingView view)const;

		[[nodiscard]] Uint32 OutputShaderResourceViewIndex(RaytracingView view)const;

	private:
		static constexpr Uint32 accumulationSlotCount_ = 2;

		struct TaauResolveConstants
		{
			Uint colorShaderResourceViewIndex_ = 0;
			Uint depthShaderResourceViewIndex_ = 0;
			Uint velocityShaderResourceViewIndex_ = 0;
			Uint historyShaderResourceViewIndex_ = 0;

			Uint destinationUnorderedAccessViewIndex_ = 0;
			Uint sourceWidth_ = 0;
			Uint sourceHeight_ = 0;
			Uint destinationWidth_ = 0;

			Uint destinationHeight_ = 0;
			Uint taauResolvePadding0_ = 0;
			Uint taauResolvePadding1_ = 0;
			Uint taauResolvePadding2_ = 0;
		};

		struct View
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> accumulatedResource_[accumulationSlotCount_];
			D3D12_RESOURCE_STATES accumulatedState_[accumulationSlotCount_] = { D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON };
			Uint32 accumulatedUnorderedAccessViewIndex_[accumulationSlotCount_] = { 0, 0 };
			Uint32 accumulatedShaderResourceViewIndex_[accumulationSlotCount_] = { 0, 0 };
			Uint32 writeSlot_ = 0;

			ResourcePtr<ConstantBuffer<TaauResolveConstants>> constantBuffer_;
		};

		TaauResolveShader resolveShader_;
		DlssBackgroundVelocityShader backgroundVelocityShader_;

		View editorView_;
		View gameView_;

		[[nodiscard]] View& ViewFor(RaytracingView view);

		void CreateViewResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 outputWidth, Uint32 outputHeight);

		BindlessHeap* bindlessHeap_ = nullptr;

		Uint32 outputWidth_ = 0;
		Uint32 outputHeight_ = 0;

		Bool pipelineStateMissingLogged_ = false;
	};
}
