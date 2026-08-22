#include <GraphicsEngine/Renderer/DlssRayReconstructionRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	DlssRayReconstructionRenderer::DlssRayReconstructionRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : normalRoughnessShader_(rootSignature, pipelineStateObject), backgroundVelocityShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void DlssRayReconstructionRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight)
	{
		indicesSystem_ = &indicesSystem;

		normalRoughnessShader_.Create(shaderCache, device);
		backgroundVelocityShader_.Create(shaderCache, device);

		CreateViewResources(device, bindlessHeap, width, height, outputWidth, outputHeight);
	}

	void DlssRayReconstructionRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		View* views[] = { &editorView_, &gameView_ };
		for (View* view : views)
		{
			bindlessHeap->FreeIndex(view->normalRoughnessUnorderedAccessViewIndex_);
			bindlessHeap->FreeIndex(view->specularAlbedoUnorderedAccessViewIndex_);
			bindlessHeap->FreeIndex(view->diffuseAlbedoUnorderedAccessViewIndex_);
			bindlessHeap->FreeIndex(view->outputUnorderedAccessViewIndex_);
			bindlessHeap->FreeIndex(view->outputShaderResourceViewIndex_);

			view->normalRoughnessResource_.Reset();
			view->specularAlbedoResource_.Reset();
			view->diffuseAlbedoResource_.Reset();
			view->outputResource_.Reset();
		}
	}

	void DlssRayReconstructionRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight)
	{
		Destroy(bindlessHeap);
		indicesSystem_ = &indicesSystem;
		CreateViewResources(device, bindlessHeap, width, height, outputWidth, outputHeight);
	}

	void DlssRayReconstructionRenderer::CreateViewResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight)
	{
		bindlessHeap_ = bindlessHeap;
		width_ = width;
		height_ = height;
		outputWidth_ = outputWidth;
		outputHeight_ = outputHeight;

		HRESULT hr{ S_OK };

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;

		View* views[] = { &editorView_, &gameView_ };
		for (View* view : views)
		{
			// ---- 法線+ラフネス(ネイティブ解像度) ----
			D3D12_RESOURCE_DESC normalRoughnessDesc{};
			normalRoughnessDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			normalRoughnessDesc.Width = width;
			normalRoughnessDesc.Height = height;
			normalRoughnessDesc.DepthOrArraySize = 1;
			normalRoughnessDesc.MipLevels = 1;
			normalRoughnessDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			normalRoughnessDesc.SampleDesc.Count = 1;
			normalRoughnessDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &normalRoughnessDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view->normalRoughnessResource_));
			SC_HR_CHECK(hr, "DLSS-RR 法線+ラフネステクスチャの生成に失敗しました");
			view->normalRoughnessState_ = D3D12_RESOURCE_STATE_COMMON;

			view->normalRoughnessUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view->normalRoughnessResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(view->normalRoughnessUnorderedAccessViewIndex_));

			D3D12_RESOURCE_DESC specularAlbedoDesc{};
			specularAlbedoDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			specularAlbedoDesc.Width = width;
			specularAlbedoDesc.Height = height;
			specularAlbedoDesc.DepthOrArraySize = 1;
			specularAlbedoDesc.MipLevels = 1;
			specularAlbedoDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			specularAlbedoDesc.SampleDesc.Count = 1;
			specularAlbedoDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &specularAlbedoDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view->specularAlbedoResource_));
			SC_HR_CHECK(hr, "DLSS-RR スペキュラアルベドテクスチャの生成に失敗しました");
			view->specularAlbedoState_ = D3D12_RESOURCE_STATE_COMMON;

			view->specularAlbedoUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view->specularAlbedoResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(view->specularAlbedoUnorderedAccessViewIndex_));

			D3D12_RESOURCE_DESC diffuseAlbedoDesc{};
			diffuseAlbedoDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			diffuseAlbedoDesc.Width = width;
			diffuseAlbedoDesc.Height = height;
			diffuseAlbedoDesc.DepthOrArraySize = 1;
			diffuseAlbedoDesc.MipLevels = 1;
			diffuseAlbedoDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			diffuseAlbedoDesc.SampleDesc.Count = 1;
			diffuseAlbedoDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &diffuseAlbedoDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view->diffuseAlbedoResource_));
			SC_HR_CHECK(hr, "DLSS-RR 拡散アルベドテクスチャの生成に失敗しました");
			view->diffuseAlbedoState_ = D3D12_RESOURCE_STATE_COMMON;

			view->diffuseAlbedoUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view->diffuseAlbedoResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(view->diffuseAlbedoUnorderedAccessViewIndex_));

			// ---- デノイズ+アップスケール出力 ----
			D3D12_RESOURCE_DESC outputDesc{};
			outputDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			outputDesc.Width = static_cast<Uint64>(outputWidth);
			outputDesc.Height = outputHeight;
			outputDesc.DepthOrArraySize = 1;
			outputDesc.MipLevels = 1;
			outputDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			outputDesc.SampleDesc.Count = 1;
			outputDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &outputDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view->outputResource_));
			SC_HR_CHECK(hr, "DLSS-RR 出力テクスチャの生成に失敗しました");
			view->outputState_ = D3D12_RESOURCE_STATE_COMMON;

			view->outputUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view->outputResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(view->outputUnorderedAccessViewIndex_));

			view->outputShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateShaderResourceView(view->outputResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(view->outputShaderResourceViewIndex_));
		}

		/// [JP] 法線+ラフネステクスチャの bindless UAV インデックスは各ビューの
		///      生存期間中不変(ピンポン無し、ビューごとの単一バッファ)。
		///      ConstantIndices::dlss_ に置いてある(ビューごとに独立した
		///      スロット)ので、Editor/Game それぞれ自分の値を持てる。
		///      この Renderer は RTエフェクト系のような Gather/PrepareFrame→
		///      Dispatch の2段構成ではなく(DLSS-RR は G-Buffer/合成の【後】、
		///      フレーム末尾で走るため、Dispatch 時点では
		///      IndicesSystem::UploadEditor/UploadGame が既にこのフレームの
		///      定数バッファを確定済み)、Create() 時点の1度だけ
		///      ConstantIndices へ登録すれば以後全フレームで有効。
		indicesSystem_->SetEditorDlssNormalRoughnessUnorderedAccessViewIndex(editorView_.normalRoughnessUnorderedAccessViewIndex_);
		indicesSystem_->SetGameDlssNormalRoughnessUnorderedAccessViewIndex(gameView_.normalRoughnessUnorderedAccessViewIndex_);
		indicesSystem_->SetEditorDlssSpecularAlbedoUnorderedAccessViewIndex(editorView_.specularAlbedoUnorderedAccessViewIndex_);
		indicesSystem_->SetGameDlssSpecularAlbedoUnorderedAccessViewIndex(gameView_.specularAlbedoUnorderedAccessViewIndex_);
		indicesSystem_->SetEditorDlssDiffuseAlbedoUnorderedAccessViewIndex(editorView_.diffuseAlbedoUnorderedAccessViewIndex_);
		indicesSystem_->SetGameDlssDiffuseAlbedoUnorderedAccessViewIndex(gameView_.diffuseAlbedoUnorderedAccessViewIndex_);
	}

	void DlssRayReconstructionRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view, DlssManager* dlssManager, const SceneConstantBuffer& scene, ID3D12Resource* colorResource, ID3D12Resource* depthResource, ID3D12Resource* velocityResource, Uint32 sourceWidth, Uint32 sourceHeight, UpscaleMode mode)
	{
		auto* cmd = cmdList->Get();
		View& target = ViewFor(view);

		ID3D12PipelineState* normalRoughnessPipelineState = normalRoughnessShader_.GetPipelineState();
		if (!normalRoughnessPipelineState && !pipelineStateMissingLogged_)
		{
			SC_LOG_WARNING("DlssNormalRoughnessCS のコンピュート PSO 作成に失敗しています。DLSS-RR は実行されません。");
			pipelineStateMissingLogged_ = true;
		}

		if (!normalRoughnessPipelineState)
		{
			return;
		}

		if (target.normalRoughnessState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(target.normalRoughnessResource_.Get(), target.normalRoughnessState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			target.normalRoughnessState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		if (target.specularAlbedoState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(target.specularAlbedoResource_.Get(), target.specularAlbedoState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			target.specularAlbedoState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		if (target.diffuseAlbedoState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(target.diffuseAlbedoResource_.Get(), target.diffuseAlbedoState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			target.diffuseAlbedoState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootSignature(normalRoughnessShader_.GetRootSignature());
		cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
		cmd->SetComputeRootConstantBufferView(2, constantIndex);
		cmd->SetComputeRootConstantBufferView(3, structuredIndex);
		cmd->SetPipelineState(normalRoughnessPipelineState);
		cmd->Dispatch((width_ + 7) / 8, (height_ + 7) / 8, 1);
		ProfilerStats::AddDrawCall();

		/// [JP] 背景(空/雲)ピクセルの速度パッチ。GBufferのRT3(velocity)は
		///      ラスタG-Bufferパスが既に PIXEL_SHADER_RESOURCE へ遷移済みなので、
		///      ここで一時的に UNORDERED_ACCESS へ戻し、パッチ後また
		///      PIXEL_SHADER_RESOURCE へ戻す(GeometryBuffer側の状態追跡と
		///      整合するよう、進入時と同じ状態で抜ける)。
		ID3D12PipelineState* backgroundVelocityPipelineState = backgroundVelocityShader_.GetPipelineState();
		if (backgroundVelocityPipelineState)
		{
			cmdList->Barrier(velocityResource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			cmd->SetPipelineState(backgroundVelocityPipelineState);
			cmd->Dispatch((width_ + 7) / 8, (height_ + 7) / 8, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(velocityResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}

		if (target.outputState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(target.outputResource_.Get(), target.outputState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			target.outputState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		DlssBufferTag tags{};
		tags.colorBuffer_ = colorResource;
		tags.depthBuffer_ = depthResource;
		tags.albedoBuffer_ = target.diffuseAlbedoResource_.Get();
		tags.velocityBuffer_ = velocityResource;
		tags.normalRoughnessBuffer_ = target.normalRoughnessResource_.Get();
		tags.specularAlbedoBuffer_ = target.specularAlbedoResource_.Get();
		tags.width_ = static_cast<Float>(sourceWidth);
		tags.height_ = static_cast<Float>(sourceHeight);

		Uint32 viewportIndex = view == RaytracingView::Editor ? 0 : 1;
		Uint32 outputWidth = outputWidth_;
		Uint32 outputHeight = outputHeight_;

		/// [JP] カメラ定数はTag()より前に設定する(Streamlineの規約)。
		///      reset=falseはカメラカット検出を今は行っていないため常に不変
		///      (既知の残課題)。
		dlssManager->Constants(scene, viewportIndex, false);
		dlssManager->Tag(tags, cmd, target.outputResource_.Get(), viewportIndex, outputWidth, outputHeight);
		dlssManager->EvaluateRayReconstruction(cmd, scene, viewportIndex, outputWidth, outputHeight, mode);

		cmdList->Barrier(target.outputResource_.Get(), target.outputState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		target.outputState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}

	ID3D12Resource* DlssRayReconstructionRenderer::OutputResource(RaytracingView view)const
	{
		return view == RaytracingView::Editor ? editorView_.outputResource_.Get() : gameView_.outputResource_.Get();
	}

	Uint32 DlssRayReconstructionRenderer::OutputShaderResourceViewIndex(RaytracingView view)const
	{
		return view == RaytracingView::Editor ? editorView_.outputShaderResourceViewIndex_ : gameView_.outputShaderResourceViewIndex_;
	}

	DlssRayReconstructionRenderer::View& DlssRayReconstructionRenderer::ViewFor(RaytracingView view)
	{
		return view == RaytracingView::Editor ? editorView_ : gameView_;
	}
}
