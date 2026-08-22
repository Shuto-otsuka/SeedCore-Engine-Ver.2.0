#include <GraphicsEngine/Renderer/VolumetricStarRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	VolumetricStarRenderer::VolumetricStarRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : starShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void VolumetricStarRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;

		width_ = width;
		height_ = height;

		starShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<VolumetricStarRayConstantBuffer>>(device, bindlessHeap);

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width_;
		resourceDesc.Height = height_;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&starResource_));
		SC_HR_CHECK(hr, "スターリソースの生成に失敗しました");
		starState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		starUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(starResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(starUnorderedAccessViewIndex_));

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);
		clearIndex_ = clearHeap_.AllocateIndex();
		device->CreateUnorderedAccessView(starResource_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearIndex_));

		starShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(starResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(starShaderResourceViewIndex_));
	}

	void VolumetricStarRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(starUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(starShaderResourceViewIndex_);
		starResource_.Reset();
	}

	void VolumetricStarRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);

		width_ = width;
		height_ = height;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width_;
		resourceDesc.Height = height_;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&starResource_));
		SC_HR_CHECK(hr, "スターリソースの生成に失敗しました");
		starState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		starUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(starResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(starUnorderedAccessViewIndex_));

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);
		clearIndex_ = clearHeap_.AllocateIndex();
		device->CreateUnorderedAccessView(starResource_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearIndex_));

		starShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(starResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(starShaderResourceViewIndex_));
	}

	Float VolumetricStarRenderer::RandomRange(Float minValue, Float maxValue)
	{
		std::uniform_real_distribution<Float> distribution(minValue, maxValue);
		return distribution(randomEngine_);
	}

	void VolumetricStarRenderer::SpawnShootingStar(Uint32 slot)
	{
		/// [JP] 始点はランダム、終点は始点を大きめの角度(50〜110度)だけ回転させて
		///      決める - 独立2点だと近い角度で終わる短い弧になりがちなので、
		///      「遠くまで飛ぶ」弧を保証する。回転軸も始点に依存しないよう
		///      ランダムに選ぶ。
		Vector3 start = Vector3(RandomRange(-1.0f, 1.0f), RandomRange(0.2f, 1.0f), RandomRange(-1.0f, 1.0f));
		start.Normalize();

		Vector3 randomVector = Vector3(RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f));
		Vector3 axis = start.Cross(randomVector);
		if (axis.LengthSquared() < 0.0001f)
		{
			axis = Vector3(1.0f, 0.0f, 0.0f);
		}
		axis.Normalize();

		Float travelAngle = DirectX::XMConvertToRadians(RandomRange(50.0f, 110.0f));
		Vector3 end = Vector3::Transform(start, Matrix::CreateFromAxisAngle(axis, travelAngle));
		if (end.y < 0.1f)
		{
			end.y = 0.1f;
		}
		end.Normalize();

		shootingStars_[slot].startDirection_ = start;
		shootingStars_[slot].endDirection_ = end;
		shootingStars_[slot].progress_ = 0.0001f;
		shootingStars_[slot].brightness_ = RandomRange(0.6f, 1.0f);

		/// [JP] 「ほんのちょっと遅く」: 従来の 0.6〜1.5秒から少し延ばす。
		shootingStarDuration_[slot] = RandomRange(1.0f, 2.0f);
	}

	void VolumetricStarRenderer::PrepareFrame(const VolumetricStarRayConstantBuffer& settings, Bool enabled, Float deltaTime, Float nightFactor)
	{
		for (Uint32 slot = 0; slot < volumetricStarMaxShootingStars_; slot++)
		{
			if (shootingStars_[slot].progress_ <= 0.0f)
			{
				continue;
			}

			shootingStars_[slot].progress_ += deltaTime / Max(shootingStarDuration_[slot], 0.01f);
			if (shootingStars_[slot].progress_ >= 1.0f)
			{
				shootingStars_[slot].progress_ = 0.0f;
			}
		}

		if (enabled && nightFactor > 0.0f)
		{
			for (Uint32 slot = 0; slot < volumetricStarMaxShootingStars_; slot++)
			{
				if (shootingStars_[slot].progress_ > 0.0f)
				{
					continue;
				}

				Float chance = settings.shootingStarChancePerSecond_ * nightFactor * deltaTime;
				if (RandomRange(0.0f, 1.0f) < chance)
				{
					SpawnShootingStar(slot);
				}

				break;
			}
		}

		VolumetricStarRayConstantBuffer uploadSettings = settings;
		uploadSettings.enabled_ = enabled ? 1 : 0;
		for (Uint32 slot = 0; slot < volumetricStarMaxShootingStars_; slot++)
		{
			uploadSettings.activeShootingStars_[slot] = shootingStars_[slot];
		}

		tuningBuffer_->Update(uploadSettings);
		indicesSystem_->SetStarRayConstantIndex(tuningBuffer_->GetIndex());
		indicesSystem_->SetStarOutputUnorderedAccessViewIndex(starUnorderedAccessViewIndex_);
		indicesSystem_->SetStarOutputShaderResourceViewIndex(starShaderResourceViewIndex_);
	}

	void VolumetricStarRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool enabled)
	{
		auto* cmd = cmdList->Get();

		if (starState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(starResource_.Get(), starState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			starState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		ID3D12PipelineState* pipelineState = starShader_.GetPipelineState();

		if (!pipelineState && !pipelineStateMissingLogged_)
		{
			SC_LOG_WARNING("VolumetricStarRT のコンピュート PSO 作成に失敗しています。星は常に無し(0)として扱われます。");
			pipelineStateMissingLogged_ = true;
		}

		if (!enabled || !pipelineState)
		{
			const Float clearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(starUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearIndex_), starResource_.Get(), clearValues, 0, nullptr);
		}
		else
		{
			ID3D12DescriptorHeap* heaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetComputeRootSignature(starShader_.GetRootSignature());
			cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
			cmd->SetComputeRootConstantBufferView(2, constantIndex);
			cmd->SetComputeRootConstantBufferView(3, structuredIndex);

			cmd->SetPipelineState(pipelineState);

			Uint32 groupCountX = (width_ + 7) / 8;
			Uint32 groupCountY = (height_ + 7) / 8;
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();
		}

		cmdList->Barrier(starResource_.Get(), starState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		starState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}
}
