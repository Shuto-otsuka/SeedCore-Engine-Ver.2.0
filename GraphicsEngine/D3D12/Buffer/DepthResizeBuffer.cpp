#include <GraphicsEngine/D3D12/Buffer/DepthResizeBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/GeometryBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void DepthResizeBuffer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height)
	{
		HRESULT hr{ S_OK };

		bindlessHeap_ = bindlessHeap;
		width_ = width;
		height_ = height;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		/// [EN] D3D12 forbids ALLOW_DEPTH_STENCIL combined with ALLOW_UNORDERED_ACCESS on one resource (see class comment) - depthResource_ is DSV-only.
		/// [JP] D3D12はALLOW_DEPTH_STENCILとALLOW_UNORDERED_ACCESSの併用を1つのリソースでは許さない(クラスコメント参照) - depthResource_はDSV専用。
		D3D12_RESOURCE_DESC depthDesc{};
		depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.DepthOrArraySize = 1;
		depthDesc.MipLevels = 1;
		depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		/// [EN] Reverse-Z (matches HiZBuffer's MIN-reduction convention - the far plane clears to 0).
		/// [JP] reverse-Z(HiZBufferのMIN縮約の慣例に合わせる - 遠平面は0にクリアされる)。
		D3D12_CLEAR_VALUE depthClearValue{};
		depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthClearValue.DepthStencil.Depth = 0.0f;
		depthClearValue.DepthStencil.Stencil = 0;

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_COMMON, &depthClearValue, IID_PPV_ARGS(&depthResource_));
		SC_HR_CHECK(hr, "深度リサイズバッファ(深度ビュー)の生成に失敗しました");
		depthState_ = D3D12_RESOURCE_STATE_COMMON;

		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
		depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device->CreateDepthStencilView(depthResource_.Get(), &depthStencilViewDesc, depthStencilViewHeap_.CPUHandle(0));

		/// [EN] uavResource_ shares depthResource_'s bit layout (both single-channel 32-bit) so CopyResource between them in Dispatch() is a plain blit, not a format conversion.
		/// [JP] uavResource_はdepthResource_とビットレイアウトが同一(どちらも単一チャンネル32ビット)なので、Dispatch()内でのCopyResourceはフォーマット変換ではなく単純なブリットになる。
		D3D12_RESOURCE_DESC uavDesc{};
		uavDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		uavDesc.Width = width;
		uavDesc.Height = height;
		uavDesc.DepthOrArraySize = 1;
		uavDesc.MipLevels = 1;
		uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
		uavDesc.SampleDesc.Count = 1;
		uavDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uavResource_));
		SC_HR_CHECK(hr, "深度リサイズバッファ(UAVビュー)の生成に失敗しました");
		uavState_ = D3D12_RESOURCE_STATE_COMMON;

		unorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		device->CreateUnorderedAccessView(uavResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(unorderedAccessViewIndex_));

		constantBuffer_ = MakePtr<ConstantBuffer<DepthResizeConstants>>(device, bindlessHeap);

		Handle<RootSignature> rootSignatureHandle = rootSignature.GetOrCreate(device);
		rootSignature_ = rootSignature.Get(rootSignatureHandle);

		resizeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Model/DepthResizeCS.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(resizeShader_)->Bytecode();
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle = pipelineStateObject.GetOrCreate(device, psokey);
		pipelineState_ = pipelineStateObject.Get(pipelineStateHandle);
	}

	void DepthResizeBuffer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(unorderedAccessViewIndex_);

		depthResource_.Reset();
		uavResource_.Reset();
		constantBuffer_ = nullptr;
	}

	void DepthResizeBuffer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);
		Create(device, bindlessHeap, shaderCache, rootSignature, pipelineStateObject, width, height);
	}

	/**
	* [EN]
	* Point-resamples geometryBuffer's native-resolution depth into
	* uavResource_ (see DepthResizeCS.hlsl), copies that into
	* depthResource_, then transitions depthResource_ to DEPTH_WRITE -
	* ready for DepthStencilViewHandle() to be bound as a fixed-function
	* depth test target immediately afterward. Internally calls
	* geometryBuffer.EndDepthNonPixel() to make the source depth
	* shader-readable first (same contract as HiZBuffer::Build), so the
	* caller must itself call geometryBuffer.BeginDepth() before its own
	* depth-testing draw - it does not come back from this call already
	* bound.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* geometryBufferのネイティブ解像度深度をuavResource_へポイントリサンプル
	* し(DepthResizeCS.hlsl参照)、それをdepthResource_へコピーしてから
	* DEPTH_WRITEへ遷移する - 直後にDepthStencilViewHandle()を固定機能の
	* 深度テストターゲットとしてバインドできる状態になる。内部で
	* geometryBuffer.EndDepthNonPixel()を呼び、先にソース深度をシェーダ
	* 読み取り可能にする(HiZBuffer::Buildと同じ契約)。そのため呼び出し側は、
	* 自身の深度テスト描画の前にgeometryBuffer.BeginDepth()を自分で呼ぶ
	* 必要がある - この呼び出しから戻った時点でバインド済みにはならない。
	*/
	void DepthResizeBuffer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, GeometryBuffer& geometryBuffer, Uint32 sourceWidth, Uint32 sourceHeight)
	{
		auto* cmd = cmdList->Get();

		geometryBuffer.EndDepthNonPixel(cmdList);

		if (uavState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(uavResource_.Get(), uavState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			uavState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		DepthResizeConstants constants{};
		constants.sourceIndex_ = geometryBuffer.DepthShaderResourceViewIndex();
		constants.destinationIndex_ = unorderedAccessViewIndex_;
		constants.destinationWidth_ = width_;
		constants.destinationHeight_ = height_;
		constants.sourceWidth_ = sourceWidth;
		constants.sourceHeight_ = sourceHeight;
		constantBuffer_->Update(constants);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootSignature(rootSignature_->Get());
		cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
		cmd->SetComputeRootConstantBufferView(2, constantBuffer_->Address());
		cmd->SetPipelineState(pipelineState_.Get());

		Uint32 dispatchX = (width_ + 7) / 8;
		Uint32 dispatchY = (height_ + 7) / 8;
		cmd->Dispatch(dispatchX, dispatchY, 1);

		cmdList->Barrier(uavResource_.Get(), uavState_, D3D12_RESOURCE_STATE_COPY_SOURCE);
		uavState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;

		cmdList->Barrier(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_COPY_DEST);
		depthState_ = D3D12_RESOURCE_STATE_COPY_DEST;

		cmd->CopyResource(depthResource_.Get(), uavResource_.Get());

		cmdList->Barrier(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DepthResizeBuffer::DepthStencilViewHandle()const
	{
		return depthStencilViewHeap_.CPUHandle(0);
	}
}
