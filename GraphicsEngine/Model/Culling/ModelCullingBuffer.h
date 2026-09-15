#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/Assert.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>

namespace SeedCore
{
	class BindlessHeap;

	struct ModelCullingStructuredBuffer
	{
		Uint instanceIndex_ = 0;
		Uint meshletIndex_ = 0;
		Uint shellIndex_ = 0;
		Uint modelCullingStructuredBufferPadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(ModelCullingStructuredBuffer, 16, "Model/Model.hlsli");

	struct ModelCullingConstantBuffer
	{
		Uint singleSidedIndex_ = 0;
		Uint doubleSidedIndex_ = 0;
		Uint argumentsIndex_ = 0;
		Uint modelCullingConstantBufferPadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(ModelCullingConstantBuffer, 16, "Model/Model.hlsli");

	class ModelCullingBuffer
	{
	public:
		ModelCullingBuffer() = default;
		~ModelCullingBuffer();

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap);

		void Reserve(Uint capacity);

		void Begin(ID3D12GraphicsCommandList* cmdList);

		void Barrier(ID3D12GraphicsCommandList* cmdList)const;

		void End(ID3D12GraphicsCommandList* cmdList)const;

		[[nodiscard]] Uint GetConstantBufferIndex()const;

		[[nodiscard]] Uint GetSingleSidedShaderResourceViewIndex()const;

		[[nodiscard]] Uint GetDoubleSidedShaderResourceViewIndex()const;

		[[nodiscard]] ID3D12CommandSignature* GetCommandSignature()const;

		[[nodiscard]] ID3D12Resource* GetArgumentBuffer()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> singleSidedBuffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> doubleSidedBuffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> argumentBuffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> argumentResetBuffer_;
		Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;

		ResourcePtr<ConstantBuffer<ModelCullingConstantBuffer>> constantBuffer_;

		Uint singleSidedUnorderedAccessViewIndex_ = 0;
		Uint singleSidedShaderResourceViewIndex_ = 0;
		Uint doubleSidedUnorderedAccessViewIndex_ = 0;
		Uint doubleSidedShaderResourceViewIndex_ = 0;
		Uint argumentUnorderedAccessViewIndex_ = 0;

		Uint capacity_ = 0;

		ID3D12Device* device_ = nullptr;
		BindlessHeap* bindlessHeap_ = nullptr;
	};
}
