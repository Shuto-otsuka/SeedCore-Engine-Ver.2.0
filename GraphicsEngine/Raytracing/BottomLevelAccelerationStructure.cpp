#include <GraphicsEngine/Raytracing/BottomLevelAccelerationStructure.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	namespace
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> CreateAccelerationStructureBuffer(ID3D12Device5* device, Uint64 size, D3D12_RESOURCE_STATES state)
		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = size;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, state, nullptr, IID_PPV_ARGS(&resource));
			SC_HR_CHECK(hr, "アクセラレーション構造用バッファの作成に失敗しました。");
			return resource;
		}
	}

	Bool BottomLevelAccelerationStructure::Build(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const BottomLevelGeometryDesc* geometries, Uint32 geometryCount)
	{
		if (geometries == nullptr || geometryCount == 0)
		{
			return false;
		}

		DynamicArray<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
		geometryDescs.reserve(geometryCount);
		for (Uint32 geometryIndex = 0; geometryIndex < geometryCount; geometryIndex++)
		{
			const BottomLevelGeometryDesc& source = geometries[geometryIndex];

			D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc{};
			geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			geometryDesc.Flags = source.opaque_ ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
			geometryDesc.Triangles.VertexBuffer.StartAddress = source.vertexBuffer_;
			geometryDesc.Triangles.VertexBuffer.StrideInBytes = source.vertexStride_;
			geometryDesc.Triangles.VertexCount = source.vertexCount_;
			geometryDesc.Triangles.VertexFormat = source.vertexFormat_;
			geometryDesc.Triangles.IndexBuffer = source.indexBuffer_;
			geometryDesc.Triangles.IndexCount = source.indexBuffer_ ? source.indexCount_ : 0;
			geometryDesc.Triangles.IndexFormat = source.indexBuffer_ ? source.indexFormat_ : DXGI_FORMAT_UNKNOWN;
			geometryDesc.Triangles.Transform3x4 = 0;
			geometryDescs.push_back(geometryDesc);
		}

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		inputs.NumDescs = geometryCount;
		inputs.pGeometryDescs = geometryDescs.data();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
		device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
		if (prebuildInfo.ResultDataMaxSizeInBytes == 0)
		{
			return false;
		}

		scratch_ = CreateAccelerationStructureBuffer(device, prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_COMMON);
		result_ = CreateAccelerationStructureBuffer(device, prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
		scratch_->SetName(L"BottomLevelAccelerationStructure_Scratch");
		result_->SetName(L"BottomLevelAccelerationStructure_Result");

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
		buildDesc.Inputs = inputs;
		buildDesc.ScratchAccelerationStructureData = scratch_->GetGPUVirtualAddress();
		buildDesc.DestAccelerationStructureData = result_->GetGPUVirtualAddress();

		commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

		/// [EN] The TLAS build and any ray query reading this BLAS must wait for the
		///      build writes to complete; a UAV barrier on the result enforces that.
		/// [JP] この BLAS を読む TLAS 構築やレイクエリは、構築書き込みの完了を待つ
		///      必要がある。result への UAV バリアでそれを保証する。
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = result_.Get();
		commandList->ResourceBarrier(1, &barrier);

		return true;
	}

	D3D12_GPU_VIRTUAL_ADDRESS BottomLevelAccelerationStructure::Address()const noexcept
	{
		return result_ ? result_->GetGPUVirtualAddress() : 0;
	}

	ID3D12Resource* BottomLevelAccelerationStructure::Resource()const noexcept
	{
		return result_.Get();
	}
}
