#include <GraphicsEngine/D3D12/Buffer/HiZBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/GeometryBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void HiZBuffer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height, Uint depthShaderResourceViewIndex)
	{
		HRESULT hr{ S_OK };

		bindlessHeap_ = bindlessHeap;

		/// [EN] Mip 0 is half the depth resolution; halve until 1x1.
		/// [JP] ミップ 0 は深度解像度の半分。1x1 まで半減を繰り返す。
		Uint mipWidth = width / 2 > 0 ? width / 2 : 1;
		Uint mipHeight = height / 2 > 0 ? height / 2 : 1;
		mipCount_ = 0;
		while (mipCount_ < maxMipCount)
		{
			mipWidths_[mipCount_] = mipWidth;
			mipHeights_[mipCount_] = mipHeight;
			mipCount_++;
			if (mipWidth == 1 && mipHeight == 1)
			{
				break;
			}
			mipWidth = mipWidth / 2 > 0 ? mipWidth / 2 : 1;
			mipHeight = mipHeight / 2 > 0 ? mipHeight / 2 : 1;
		}

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = mipWidths_[0];
		resourceDesc.Height = mipHeights_[0];
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = static_cast<UINT16>(mipCount_);
		resourceDesc.Format = DXGI_FORMAT_R32_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource_));
		SC_HR_CHECK(hr, "HiZバッファの生成に失敗しました");
		state_ = D3D12_RESOURCE_STATE_COMMON;

		/// [EN] Per-mip UAVs for the build passes.
		/// [JP] 構築パス用のミップごとの UAV。
		for (Uint mip = 0; mip < mipCount_; mip++)
		{
			mipUnorderedAccessViewIndices_[mip] = bindlessHeap->AllocateIndex();
			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			unorderedAccessViewDesc.Texture2D.MipSlice = mip;
			device->CreateUnorderedAccessView(resource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(mipUnorderedAccessViewIndices_[mip]));
		}

		/// [EN] Full-chain SRV for the occlusion test in the Amplification Shader.
		/// [JP] Amplification Shader でのオクルージョンテスト用フルチェーン SRV。
		shaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = mipCount_;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		device->CreateShaderResourceView(resource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(shaderResourceViewIndex_));

		/// [EN] One small constant buffer per mip pass. The buffers are
		///      frame-ringed, so Build re-writes the current slot every frame.
		/// [JP] ミップパスごとに小さな定数バッファを 1 つ。バッファはフレーム
		///      リングなので、Build が毎フレーム現在スロットに書き直す。
		mipConstantBuffers_.resize(mipCount_);
		mipConstants_.resize(mipCount_);
		for (Uint mip = 0; mip < mipCount_; mip++)
		{
			mipConstantBuffers_[mip] = MakePtr<ConstantBuffer<HiZBuildConstants>>(device, bindlessHeap);

			HiZBuildConstants constants{};
			constants.destinationIndex_ = mipUnorderedAccessViewIndices_[mip];
			constants.destinationWidth_ = mipWidths_[mip];
			constants.destinationHeight_ = mipHeights_[mip];
			if (mip == 0)
			{
				constants.sourceIndex_ = depthShaderResourceViewIndex;
				constants.sourceWidth_ = width;
				constants.sourceHeight_ = height;
				constants.sourceIsDepth_ = 1;
			}
			else
			{
				constants.sourceIndex_ = mipUnorderedAccessViewIndices_[mip - 1];
				constants.sourceWidth_ = mipWidths_[mip - 1];
				constants.sourceHeight_ = mipHeights_[mip - 1];
				constants.sourceIsDepth_ = 0;
			}
			mipConstants_[mip] = constants;
			mipConstantBuffers_[mip]->Update(constants);
		}

		Handle<RootSignature> rootSignatureHandle = rootSignature.GetOrCreate(device);
		rootSignature_ = rootSignature.Get(rootSignatureHandle);

		buildShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Model/HiZBufferCS.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(buildShader_)->Bytecode();
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle = pipelineStateObject.GetOrCreate(device, psokey);
		pipelineState_ = pipelineStateObject.Get(pipelineStateHandle);
	}

	void HiZBuffer::Destroy(BindlessHeap* bindlessHeap)
	{
		for (Uint mip = 0; mip < mipCount_; mip++)
		{
			bindlessHeap->FreeIndex(mipUnorderedAccessViewIndices_[mip]);
		}
		bindlessHeap->FreeIndex(shaderResourceViewIndex_);

		resource_.Reset();
		mipConstantBuffers_.clear();
		mipConstants_.clear();
		mipCount_ = 0;
	}

	void HiZBuffer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height, Uint depthShaderResourceViewIndex)
	{
		Destroy(bindlessHeap);
		Create(device, bindlessHeap, shaderCache, rootSignature, pipelineStateObject, width, height, depthShaderResourceViewIndex);
	}

	void HiZBuffer::Build(D3D12CommandList* cmdList, GeometryBuffer& geometryBuffer, ID3D12DescriptorHeap* heap)
	{
		auto* cmd = cmdList->Get();

		/// [EN] Depth prepass result must be shader-readable for the reduction.
		/// [JP] 縮小処理のためデプスプリパス結果をシェーダ読み取り可能にする。
		geometryBuffer.EndDepthNonPixel(cmdList);

		if (state_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(resource_.Get(), state_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			state_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootSignature(rootSignature_->Get());
		cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
		cmd->SetPipelineState(pipelineState_.Get());

		for (Uint mip = 0; mip < mipCount_; mip++)
		{
			mipConstantBuffers_[mip]->Update(mipConstants_[mip]);
			cmd->SetComputeRootConstantBufferView(2, mipConstantBuffers_[mip]->Address());

			Uint dispatchX = (mipWidths_[mip] + 7) / 8;
			Uint dispatchY = (mipHeights_[mip] + 7) / 8;
			cmd->Dispatch(dispatchX, dispatchY, 1);

			/// [EN] Each mip reads the previous one — serialise the passes.
			/// [JP] 各ミップは前段を読む — パス間を直列化する。
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.UAV.pResource = resource_.Get();
			cmd->ResourceBarrier(1, &barrier);
		}

		/// [EN] Readable by the following Amplification Shader passes.
		/// [JP] 後続の Amplification Shader パスから読める状態にする。
		cmdList->Barrier(resource_.Get(), state_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		state_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		/// [EN] Depth goes back to DEPTH_WRITE for the G-Buffer pass.
		/// [JP] G-Buffer パスに向けてデプスを DEPTH_WRITE に戻す。
		geometryBuffer.BeginDepth(cmdList);
	}
}
