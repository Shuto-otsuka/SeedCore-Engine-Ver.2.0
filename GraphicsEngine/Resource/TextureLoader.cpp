#include <GraphicsEngine/Resource/TextureLoader.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	void TextureLoader::CreateTexture(in ID3D12Device* device, in ID3D12CommandQueue* cmdQueue, in ID3D12DescriptorHeap* heap, in String filePath, inout Microsoft::WRL::ComPtr<ID3D12Resource>& resource, in Uint textureIndex)
	{
		HRESULT hr{ S_OK };

		DirectX::ResourceUploadBatch resourceUpload(device);
		resourceUpload.Begin();

		hr = DirectX::CreateDDSTextureFromFile(device, resourceUpload, filePath.w_str().c_str(), &resource);

		if (FAILED(hr))
		{
			hr = DirectX::CreateWICTextureFromFile(device, resourceUpload, filePath.w_str().c_str(), &resource, false);
		}

		if (FAILED(hr) || !resource)
		{
			/// [EN] Bail out instead of dereferencing a null resource below.
			/// [JP] 下で null リソースを参照しないよう、ここで中断する。
			SC_LOG_ERROR("テクスチャのロードに失敗しました: {}", filePath.str());
			auto uploadAborted = resourceUpload.End(cmdQueue);
			uploadAborted.wait();
			return;
		}

		auto uploadFinished = resourceUpload.End(cmdQueue);
		uploadFinished.wait();

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = resource->GetDesc().Format;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
		Uint64 descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuHandle.ptr += textureIndex * descriptorSize;

		device->CreateShaderResourceView(resource.Get(), &shaderResourceViewDesc, cpuHandle);
	}
}
