#include <GraphicsEngine/D3D12/Buffer/ReservoirBuffer.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void ReservoirBuffer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 elementCount, Uint32 elementSizeInBytes, Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32& outShaderResourceViewIndex, Uint32& outClearIndex)
	{
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = static_cast<Uint64>(elementSizeInBytes) * elementCount;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outResource));
		SC_HR_CHECK(hr, "ReSTIR Reservoir バッファの生成に失敗しました");

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		unorderedAccessViewDesc.Buffer.FirstElement = 0;
		unorderedAccessViewDesc.Buffer.NumElements = elementCount;
		unorderedAccessViewDesc.Buffer.StructureByteStride = elementSizeInBytes;
		unorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		outUnorderedAccessViewIndex = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outUnorderedAccessViewIndex));

		outClearIndex = clearHeap.AllocateIndex();
		device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, clearHeap.CPUHandle(outClearIndex));

		outShaderResourceViewIndex = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Buffer.FirstElement = 0;
		shaderResourceViewDesc.Buffer.NumElements = elementCount;
		shaderResourceViewDesc.Buffer.StructureByteStride = elementSizeInBytes;
		device->CreateShaderResourceView(outResource.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(outShaderResourceViewIndex));
	}
}
