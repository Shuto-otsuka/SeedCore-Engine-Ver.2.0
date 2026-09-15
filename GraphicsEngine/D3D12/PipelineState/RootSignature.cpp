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

		D3D12_ROOT_PARAMETER params[4]{};

		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[0].Descriptor.ShaderRegister = 0;
		params[0].Descriptor.RegisterSpace = 1;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[1].Descriptor.ShaderRegister = 1;
		params[1].Descriptor.RegisterSpace = 1;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[2].Descriptor.ShaderRegister = 2;
		params[2].Descriptor.RegisterSpace = 1;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[3].Constants.ShaderRegister = 4;
		params[3].Constants.RegisterSpace = 1;
		params[3].Constants.Num32BitValues = 1;
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

	void RootSignature::BindCompute(ID3D12GraphicsCommandList* cmd, const RootAddresses& addresses)
	{
		cmd->SetComputeRootConstantBufferView(0, addresses.shaderResource_);
		cmd->SetComputeRootConstantBufferView(1, addresses.unorderedAccess_);
		cmd->SetComputeRootConstantBufferView(2, addresses.constant_);
	}

	void RootSignature::BindGraphics(ID3D12GraphicsCommandList* cmd, const RootAddresses& addresses)
	{
		cmd->SetGraphicsRootConstantBufferView(0, addresses.shaderResource_);
		cmd->SetGraphicsRootConstantBufferView(1, addresses.unorderedAccess_);
		cmd->SetGraphicsRootConstantBufferView(2, addresses.constant_);
	}
}