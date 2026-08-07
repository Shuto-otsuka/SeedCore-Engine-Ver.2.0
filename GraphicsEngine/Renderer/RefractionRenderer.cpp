#include <GraphicsEngine/Renderer/RefractionRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	RefractionRenderer::RefractionRenderer(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject) : refractionShader_(rootSignature, raytracingStateObject)
	{
		/// No Code
	}

	/**
	* [EN]
	* Creates the single output radiance target, the tuning constant buffer,
	* and the 3-record shader table.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単一の出力放射輝度ターゲット、チューニング用定数バッファ、3 レコードの
	* シェーダテーブルを生成する。
	*/
	void RefractionRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;
		width_ = width;
		height_ = height;

		/// [JP] デバイスは常に ID3D12Device5 として生成されている(D3D12Device 参照)。
		ID3D12Device5* device5 = static_cast<ID3D12Device5*>(device);

		refractionShader_.Create(shaderCache, device5);

		tuningBuffer_ = MakePtr<ConstantBuffer<RefractionRayConstantBuffer>>(device, bindlessHeap);

		HRESULT hr{ S_OK };

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outputResource_));
		SC_HR_CHECK(hr, "屈折放射輝度テクスチャの生成に失敗しました");
		outputState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		outputUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(outputResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outputUnorderedAccessViewIndex_));

		clearOutputIndex_ = clearHeap_.AllocateIndex();
		device->CreateUnorderedAccessView(outputResource_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearOutputIndex_));

		outputShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(outputResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(outputShaderResourceViewIndex_));

		/// [JP] シェーダテーブル構築。raygenerationのみ(miss/hitgroup無し)なので
		///      1レコードだけでよい - ReflectionRenderer::Create と違い
		///      DispatchRays の MissShaderTable/HitGroupTable は使わない
		///      (Dispatch() で StartAddress=0/SizeInBytes=0 のまま渡す)。
		ID3D12StateObject* stateObject = refractionShader_.GetStateObject();
		if (!stateObject)
		{
			return;
		}

		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
		hr = stateObject->QueryInterface(IID_PPV_ARGS(&stateObjectProperties));
		if (FAILED(hr))
		{
			return;
		}

		void* rayGenIdentifier = stateObjectProperties->GetShaderIdentifier(String(RefractionShader::rayGenExportName).w_str().c_str());
		if (!rayGenIdentifier)
		{
			SC_LOG_ERROR("屈折RTPSOからシェーダ識別子(RefractionRayGeneration)を取得できませんでした");
			return;
		}

		D3D12_HEAP_PROPERTIES uploadHeapProperties{};
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC tableDesc{};
		tableDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		tableDesc.Width = shaderTableRecordSize;
		tableDesc.Height = 1;
		tableDesc.DepthOrArraySize = 1;
		tableDesc.MipLevels = 1;
		tableDesc.Format = DXGI_FORMAT_UNKNOWN;
		tableDesc.SampleDesc.Count = 1;
		tableDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &tableDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&shaderTableResource_));
		SC_HR_CHECK(hr, "屈折シェーダーテーブルリソースの生成に失敗しました");

		Uint8* mapped = nullptr;
		hr = shaderTableResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
		SC_HR_CHECK(hr, "屈折シェーダーテーブルリソースのMapに失敗しました");
		memset(mapped, 0, shaderTableRecordSize);
		memcpy(mapped, rayGenIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		shaderTableResource_->Unmap(0, nullptr);
	}

	void RefractionRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(outputUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(outputShaderResourceViewIndex_);

		// [JP] Resize() はこれを呼んだ直後に同じ幅/高さで作り直すが、その時点で
		//      前フレームのGPUコマンドがまだこのリソースを読み書きしている
		//      可能性がある(OITBuffer::Destroyと同じ理由) - 即座に.Reset()すると
		//      解放直後のメモリへ新しいリソースが再割り当てされ、GPU側が古い
		//      コマンドで新リソースを踏みに行く事故になるため遅延破棄する。
		bindlessHeap->DeferRelease(outputResource_);
		outputResource_.Reset();
	}

	void RefractionRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);
		width_ = width;
		height_ = height;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outputResource_));
		SC_HR_CHECK(hr, "屈折放射輝度テクスチャの再生成に失敗しました");
		outputState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		outputUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(outputResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outputUnorderedAccessViewIndex_));

		clearOutputIndex_ = clearHeap_.AllocateIndex();
		device->CreateUnorderedAccessView(outputResource_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearOutputIndex_));

		outputShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(outputResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(outputShaderResourceViewIndex_));
	}

	void RefractionRenderer::PrepareFrame(const RefractionRayConstantBuffer& settings)
	{
		tuningBuffer_->Update(settings);
		indicesSystem_->SetRefractionRayConstantIndex(tuningBuffer_->GetIndex());
		indicesSystem_->SetRefractionOutputUnorderedAccessViewIndex(outputUnorderedAccessViewIndex_);
		indicesSystem_->SetRefractionOutputShaderResourceViewIndex(outputShaderResourceViewIndex_);
	}

	void RefractionRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid)
	{
		auto* cmd = cmdList->Get();

		ID3D12StateObject* stateObject = refractionShader_.GetStateObject();

		if ((!stateObject || !shaderTableResource_) && !stateObjectMissingLogged_)
		{
			SC_LOG_WARNING("RefractionRT の RTPSO/シェーダテーブル作成に失敗しています。DXR(DispatchRays)非対応の可能性があります。屈折は常に無し(0)として扱われます。");
			stateObjectMissingLogged_ = true;
		}

		if (!tlasValid || !stateObject || !shaderTableResource_)
		{
			if (outputState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(outputResource_.Get(), outputState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				outputState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Float clearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(outputUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearOutputIndex_), outputResource_.Get(), clearValues, 0, nullptr);

			cmdList->Barrier(outputResource_.Get(), outputState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			outputState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			return;
		}

		if (outputState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(outputResource_.Get(), outputState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			outputState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootSignature(refractionShader_.GetRootSignature());
		cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
		cmd->SetComputeRootConstantBufferView(2, constantIndex);
		cmd->SetComputeRootConstantBufferView(3, structuredIndex);
		cmd->SetPipelineState1(stateObject);

		D3D12_GPU_VIRTUAL_ADDRESS tableAddress = shaderTableResource_->GetGPUVirtualAddress();

		// raygenerationのみ(miss/hitgroup無し) - MissShaderTable/HitGroupTable は
		// StartAddress=0/SizeInBytes=0 のまま(未使用)。
		D3D12_DISPATCH_RAYS_DESC dispatchDesc{};
		dispatchDesc.RayGenerationShaderRecord.StartAddress = tableAddress;
		dispatchDesc.RayGenerationShaderRecord.SizeInBytes = shaderTableRecordSize;
		dispatchDesc.Width = width_;
		dispatchDesc.Height = height_;
		dispatchDesc.Depth = 1;

		cmd->DispatchRays(&dispatchDesc);
		ProfilerStats::AddDrawCall();

		cmdList->Barrier(outputResource_.Get(), outputState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		outputState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
}
