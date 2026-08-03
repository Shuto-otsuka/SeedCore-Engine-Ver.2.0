#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/SamplerState.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Pool/StablePool.h>

namespace SeedCore
{
	namespace
	{
		struct RootSignatureCache
		{
			StablePool<RootSignature> pool_;
			Handle<RootSignature> handle_;
		};
		RootSignatureCache cache_;
	}

	Handle<RootSignature> RootSignature::GetOrCreate(ID3D12Device* device)
	{
		if (cache_.handle_.exists())
		{
			return cache_.handle_;
		}

		HRESULT hr{ S_OK };

		DynamicArray<D3D12_STATIC_SAMPLER_DESC> samplerDesc = SamplerState::Create();

		D3D12_DESCRIPTOR_RANGE shaderResourceViewRange{};
		shaderResourceViewRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		shaderResourceViewRange.NumDescriptors = 0xFFFFFFFF;
		shaderResourceViewRange.BaseShaderRegister = 0;
		shaderResourceViewRange.RegisterSpace = 0;
		shaderResourceViewRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE unorderedAccessViewRange{};
		unorderedAccessViewRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		unorderedAccessViewRange.NumDescriptors = 0xFFFFFFFF;
		unorderedAccessViewRange.BaseShaderRegister = 0;
		unorderedAccessViewRange.RegisterSpace = 0;
		unorderedAccessViewRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER params[4]{};

		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[0].DescriptorTable.NumDescriptorRanges = 1;
		params[0].DescriptorTable.pDescriptorRanges = &shaderResourceViewRange;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &unorderedAccessViewRange;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[2].Descriptor.ShaderRegister = 0;
		params[2].Descriptor.RegisterSpace = 1;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[3].Descriptor.ShaderRegister = 1;
		params[3].Descriptor.RegisterSpace = 1;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 4;
		rootSignatureDesc.pParameters = params;
		rootSignatureDesc.NumStaticSamplers = static_cast<Uint>(samplerDesc.size());
		rootSignatureDesc.pStaticSamplers = samplerDesc.data();
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSignature, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA(static_cast<const Char*>(errorBlob->GetBufferPointer()));
			}
			return Handle<RootSignature>::null();
		}

		Microsoft::WRL::ComPtr<ID3D12RootSignature> cacheRootSignature;
		hr = device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&cacheRootSignature));
		SC_HR_CHECK(hr, "RootSignatureの生成に失敗しました");

		auto handle = cache_.pool_.Create();
		if (auto rootSignature = cache_.pool_.Get(handle))
		{
			rootSignature->rootSignature_ = std::move(cacheRootSignature);
		}

		cache_.handle_ = handle;
		return handle;
	}

	RootSignature* RootSignature::Get(const Handle<RootSignature>& handle)
	{
		return cache_.pool_.Get(handle);
	}

	ID3D12RootSignature* RootSignature::Get()const noexcept
	{
		return rootSignature_.Get();
	}

	void RootSignature::ClearCache()
	{
		if (cache_.handle_.exists())
		{
			cache_.pool_.Destroy(cache_.handle_);
			cache_.handle_ = Handle<RootSignature>::null();
		}
	}
}