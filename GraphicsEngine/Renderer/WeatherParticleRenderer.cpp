#include <GraphicsEngine/Renderer/WeatherParticleRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <GraphicsEngine/D3D12/Buffer/FrameBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/GeometryBuffer.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	WeatherParticleRenderer::WeatherParticleRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : particleShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void WeatherParticleRenderer::CreateParticleBuffer(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 capacity, Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32& outShaderResourceViewIndex)
	{
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(WeatherParticle) * static_cast<Uint64>(capacity);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outResource));
		SC_HR_CHECK(hr, "天候パーティクルバッファの生成に失敗しました");

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		unorderedAccessViewDesc.Buffer.FirstElement = 0;
		unorderedAccessViewDesc.Buffer.NumElements = capacity;
		unorderedAccessViewDesc.Buffer.StructureByteStride = sizeof(WeatherParticle);

		outUnorderedAccessViewIndex = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outUnorderedAccessViewIndex));

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Buffer.FirstElement = 0;
		shaderResourceViewDesc.Buffer.NumElements = capacity;
		shaderResourceViewDesc.Buffer.StructureByteStride = sizeof(WeatherParticle);

		outShaderResourceViewIndex = bindlessHeap->AllocateIndex();
		device->CreateShaderResourceView(outResource.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(outShaderResourceViewIndex));
	}

	void WeatherParticleRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;

		particleShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<WeatherParticleConstantBuffer>>(device, bindlessHeap);

		CreateParticleBuffer(device, bindlessHeap, rainCapacity_, rainParticleResource_, rainParticleUnorderedAccessViewIndex_, rainParticleShaderResourceViewIndex_);
		CreateParticleBuffer(device, bindlessHeap, snowCapacity_, snowParticleResource_, snowParticleUnorderedAccessViewIndex_, snowParticleShaderResourceViewIndex_);
		rainParticleState_ = D3D12_RESOURCE_STATE_COMMON;
		snowParticleState_ = D3D12_RESOURCE_STATE_COMMON;
	}

	void WeatherParticleRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(rainParticleUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(rainParticleShaderResourceViewIndex_);
		bindlessHeap->FreeIndex(snowParticleUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(snowParticleShaderResourceViewIndex_);
		rainParticleResource_.Reset();
		snowParticleResource_.Reset();
	}

	void WeatherParticleRenderer::PrepareFrame(const Vector3& cameraPosition, Float deltaTime, Float totalTime, const Vector3& wind, Bool rainEnabled, const RainSettings& rainSettings, Float rainAmount, Bool snowEnabled, const SnowSettings& snowSettings, Float snowAmount)
	{
		WeatherParticleConstantBuffer settings;
		settings.cameraPosition_ = cameraPosition;
		settings.deltaTime_ = deltaTime;
		settings.wind_ = wind;
		settings.totalTime_ = totalTime;
		settings.forceRespawn_ = initialized_ ? 0u : 1u;

		settings.rainCapacity_ = rainCapacity_;
		settings.rainActiveCount_ = rainEnabled ? static_cast<Uint32>(rainCapacity_ * std::clamp(rainSettings.density_, 0.0f, 1.0f) * std::clamp(rainAmount, 0.0f, 1.0f)) : 0;
		settings.rainFallSpeed_ = rainSettings.fallSpeed_;
		settings.rainSize_ = rainSettings.size_;
		settings.rainStreakLength_ = rainSettings.streakLength_;
		settings.rainBrightness_ = rainSettings.brightness_;
		settings.rainVolumeRadius_ = rainSettings.volumeRadius_;
		settings.rainVolumeHeight_ = rainSettings.volumeHeight_;
		settings.rainColor_ = Vector3(rainSettings.color_[0], rainSettings.color_[1], rainSettings.color_[2]);

		settings.snowCapacity_ = snowCapacity_;
		settings.snowActiveCount_ = snowEnabled ? static_cast<Uint32>(snowCapacity_ * std::clamp(snowSettings.density_, 0.0f, 1.0f) * std::clamp(snowAmount, 0.0f, 1.0f)) : 0;
		settings.snowFallSpeed_ = snowSettings.fallSpeed_;
		settings.snowSwayAmount_ = snowSettings.swayAmount_;
		settings.snowSize_ = snowSettings.size_;
		settings.snowBrightness_ = snowSettings.brightness_;
		settings.snowVolumeRadius_ = snowSettings.volumeRadius_;
		settings.snowVolumeHeight_ = snowSettings.volumeHeight_;
		settings.snowColor_ = Vector3(snowSettings.color_[0], snowSettings.color_[1], snowSettings.color_[2]);

		activeTotal_ = settings.rainActiveCount_ + settings.snowActiveCount_;

		tuningBuffer_->Update(settings);
		indicesSystem_->SetWeatherParticleRayConstantIndex(tuningBuffer_->GetIndex());
		indicesSystem_->SetRainParticleUnorderedAccessViewIndex(rainParticleUnorderedAccessViewIndex_);
		indicesSystem_->SetRainParticleShaderResourceViewIndex(rainParticleShaderResourceViewIndex_);
		indicesSystem_->SetSnowParticleUnorderedAccessViewIndex(snowParticleUnorderedAccessViewIndex_);
		indicesSystem_->SetSnowParticleShaderResourceViewIndex(snowParticleShaderResourceViewIndex_);

		initialized_ = true;
	}

	void WeatherParticleRenderer::Simulate(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		auto* cmd = cmdList->Get();

		if (rainParticleState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(rainParticleResource_.Get(), rainParticleState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			rainParticleState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
		if (snowParticleState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(snowParticleResource_.Get(), snowParticleState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			snowParticleState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		ID3D12PipelineState* pipelineState = particleShader_.GetSimulatePipelineState();
		if (!pipelineState)
		{
			if (!pipelineStateMissingLogged_)
			{
				SC_LOG_WARNING("WeatherParticleSimulateCS のコンピュート PSO 作成に失敗しています。天候パーティクルは動作しません。");
				pipelineStateMissingLogged_ = true;
			}
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootSignature(particleShader_.GetRootSignature());
		cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
		cmd->SetComputeRootConstantBufferView(2, constantIndex);
		cmd->SetComputeRootConstantBufferView(3, structuredIndex);
		cmd->SetPipelineState(pipelineState);

		Uint32 totalCapacity = rainCapacity_ + snowCapacity_;
		Uint32 groupCount = (totalCapacity + 63) / 64;
		cmd->Dispatch(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void WeatherParticleRenderer::Draw(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (activeTotal_ == 0)
		{
			return;
		}

		ID3D12PipelineState* pipelineState = particleShader_.GetDrawPipelineState();
		if (!pipelineState)
		{
			return;
		}

		if (rainParticleState_ != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		{
			cmdList->Barrier(rainParticleResource_.Get(), rainParticleState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			rainParticleState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
		if (snowParticleState_ != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		{
			cmdList->Barrier(snowParticleResource_.Get(), snowParticleState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			snowParticleState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		auto* cmd = cmdList->Get();

		/// [JP] frameBuffer の色 + geometryBuffer の深度(読み取りのみ、PSO側で
		///      DepthWriteMask オフ)を直接バインド - ModelRenderer::
		///      DrawWireframe と同じ方式。
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = frameBuffer->RenderTargetViewHandle();
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = geometryBuffer->DepthStencilViewHandle();
		cmd->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &depthStencilViewHandle);

		D3D12_VIEWPORT viewport = frameBuffer->GetViewport();
		cmd->RSSetViewports(1, &viewport);
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
		cmd->RSSetScissorRects(1, &scissorRect);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(particleShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetPipelineState(pipelineState);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint32 groupCount = (activeTotal_ + 31) / 32;
		cmd->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}
}
