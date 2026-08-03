#include <GraphicsEngine/Raytracing/TopLevelAccelerationStructure.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	namespace
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> CreateAccelerationStructureBuffer(ID3D12Device5* device, Uint64 size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state)
		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = heapType;
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
			resourceDesc.Flags = (heapType == D3D12_HEAP_TYPE_DEFAULT) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, state, nullptr, IID_PPV_ARGS(&resource));
			SC_HR_CHECK(hr, "アクセラレーション構造用バッファの作成に失敗しました。");
			return resource;
		}
	}

	Bool TopLevelAccelerationStructure::Build(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const TopLevelInstanceDesc* instances, Uint32 instanceCount)
	{
		if (instances == nullptr || instanceCount == 0)
		{
			return false;
		}

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		inputs.NumDescs = instanceCount;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
		device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
		if (prebuildInfo.ResultDataMaxSizeInBytes == 0)
		{
			return false;
		}

		/// [EN] Frame-ring slot: Build() is called once per frame, so writing into
		///      the slot for the CURRENT frame never touches the slot the GPU may
		///      still be reading (previous frame's ray queries) for frameCount-1
		///      more frames.
		/// [JP] フレームリングスロット: Build() は毎フレーム 1 回呼ばれるため、
		///      現在フレームのスロットへ書き込んでも、GPU がまだ読んでいる
		///      可能性のある（前フレームのレイクエリの）スロットには
		///      frameCount-1 フレームの間触れない。
		Uint frame = FrameRing::Index();

		/// [EN] Upload heap holding the per-instance descriptors the build consumes.
		/// [JP] 構築が読む per-instance ディスクリプタを保持するアップロードバッファ。
		const Uint64 instanceBufferSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<Uint64>(instanceCount);
		instanceBuffer_[frame] = CreateAccelerationStructureBuffer(device, instanceBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_COMMON);

		D3D12_RAYTRACING_INSTANCE_DESC* mapped = nullptr;
		HRESULT hr = instanceBuffer_[frame]->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
		SC_HR_CHECK(hr, "TLAS インスタンスバッファのマップに失敗しました。");

		for (Uint32 index = 0; index < instanceCount; index++)
		{
			const TopLevelInstanceDesc& source = instances[index];

			D3D12_RAYTRACING_INSTANCE_DESC& destination = mapped[index];
			std::memcpy(destination.Transform, source.transform_, sizeof(destination.Transform));
			destination.InstanceID = source.instanceID_ & 0xFFFFFF;
			destination.InstanceMask = source.instanceMask_;
			destination.InstanceContributionToHitGroupIndex = source.hitGroupIndex_ & 0xFFFFFF;
			destination.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			destination.AccelerationStructure = source.bottomLevelAddress_;
		}

		instanceBuffer_[frame]->Unmap(0, nullptr);

		inputs.InstanceDescs = instanceBuffer_[frame]->GetGPUVirtualAddress();

		scratch_[frame] = CreateAccelerationStructureBuffer(device, prebuildInfo.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
		result_[frame] = CreateAccelerationStructureBuffer(device, prebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
		buildDesc.Inputs = inputs;
		buildDesc.ScratchAccelerationStructureData = scratch_[frame]->GetGPUVirtualAddress();
		buildDesc.DestAccelerationStructureData = result_[frame]->GetGPUVirtualAddress();

		commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

		/// [EN] Ray queries reading this TLAS must wait for the build to finish.
		/// [JP] この TLAS を読むレイクエリは構築完了を待つ必要がある。
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = result_[frame].Get();
		commandList->ResourceBarrier(1, &barrier);

		return true;
	}

	D3D12_GPU_VIRTUAL_ADDRESS TopLevelAccelerationStructure::Address()const noexcept
	{
		const auto& resource = result_[FrameRing::Index()];
		return resource ? resource->GetGPUVirtualAddress() : 0;
	}

	ID3D12Resource* TopLevelAccelerationStructure::Resource()const noexcept
	{
		return result_[FrameRing::Index()].Get();
	}
}
