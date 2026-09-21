#include <GraphicsEngine/Renderer/AvatarRenderer.h>
#include <GraphicsEngine/Avatar/AvatarMesh.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/Context/D3D12Check.h>

namespace SeedCore
{
	AvatarRenderer::AvatarRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : modelShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	AvatarRenderer::~AvatarRenderer() = default;

	void AvatarRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;

		modelShader_.Create(shaderCache, device);

		instanceBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ModelStructuredBuffer>>(device, bindlessHeap, maxInstanceCount_);
		boneBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Matrix>>(device, bindlessHeap, maxBoneCount_);

		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		frameBuffer_ = MakePtr<FrameBuffer>(device, &renderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &depthStencilViewHeap_, 0.45f, 0.65f, 0.9f, 1.0f);

		sceneSystem_ = MakePtr<SceneSystem>(device, bindlessHeap);

		constantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, bindlessHeap);
		shaderResourceIndicesBuffer_ = MakePtr<ConstantBuffer<ShaderResourceIndices>>(device, bindlessHeap);

		if (D3D12Check::GetLevel() != D3D12Level::D12_2)
		{
			modelCullingBuffer_.Create(device, bindlessHeap);
		}
	}

	void AvatarRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		frameBuffer_->Resize(device, bindlessHeap, width, height);
	}

	void AvatarRenderer::Gather(const AvatarMesh& mesh, Uint32 boneCount, const Matrix& worldMatrix, std::span<const Uint32> regionTextureIndices)
	{
		instances_.clear();
		boneMatrices_.clear();
		uploaded_ = false;

		if (!mesh.Created() || mesh.MeshletCount() == 0)
		{
			return;
		}

		if (boneCount == 0 || boneCount > maxBoneCount_)
		{
			return;
		}
		boneMatrices_.assign(boneCount, Matrix::Identity);

		Matrix inverseTransposeWorld = worldMatrix.Invert().Transpose();

		for (Uint32 regionIndex = 0; regionIndex < mesh.RegionCount(); regionIndex++)
		{
			const AvatarMesh::RegionMeshletRange& regionRange = mesh.MeshletRangeForRegion(regionIndex);
			Uint32 regionTextureIndex = regionIndex < regionTextureIndices.size() ? regionTextureIndices[regionIndex] : 0xFFFFFFFF;

			for (Uint32 localOffset = 0; localOffset < regionRange.meshletCount_; localOffset += maxMeshletsPerDispatch_)
			{
				Uint32 count = Min(maxMeshletsPerDispatch_, regionRange.meshletCount_ - localOffset);

				ModelStructuredBuffer instanceData{};
				instanceData.transform_.world_ = worldMatrix;
				instanceData.transform_.inverseTransposeWorld_ = inverseTransposeWorld;
				instanceData.transform_.previousWorld_ = worldMatrix;

				instanceData.texture_.baseColor_ = Color(0.78f, 0.76f, 0.74f, 1.0f);
				instanceData.texture_.metallic_ = 0.0f;
				instanceData.texture_.roughness_ = 0.85f;
				instanceData.texture_.alphaCutoff_ = 0.0f;
				instanceData.shading_.shadingModel_ = static_cast<Uint>(ShadingModel::Lambert);
				instanceData.extension_.unlit_ = 0.0f;

				instanceData.texture_.baseColorTextureIndex_ = regionTextureIndex;
				instanceData.texture_.normalTextureIndex_ = 0xFFFFFFFF;
				instanceData.texture_.metallicRoughnessTextureIndex_ = 0xFFFFFFFF;
				instanceData.texture_.emissiveTextureIndex_ = 0xFFFFFFFF;
				instanceData.texture_.occlusionTextureIndex_ = 0xFFFFFFFF;

				instanceData.geometry_.vertexBufferIndex_ = mesh.VertexBufferIndex();
				instanceData.geometry_.meshletBufferIndex_ = mesh.MeshletBufferIndex();
				instanceData.geometry_.meshletBoundBufferIndex_ = mesh.MeshletBoundBufferIndex();
				instanceData.geometry_.vertexIndicesBufferIndex_ = mesh.VertexIndicesBufferIndex();
				instanceData.geometry_.primitiveIndicesBufferIndex_ = mesh.PrimitiveIndicesBufferIndex();
				instanceData.skining_.skinVertexBufferIndex_ = 0xFFFFFFFF;

				instanceData.geometry_.meshletOffset_ = regionRange.meshletOffset_ + localOffset;
				instanceData.geometry_.meshletCount_ = count;

				instanceData.skining_.skinIndex_ = 0xFFFFFFFF;
				instanceData.skining_.boneOffset_ = 0;

				instanceData.streaming_.positionMin_ = mesh.PositionMin();
				instanceData.streaming_.positionExtent_ = mesh.PositionExtent();
				instanceData.streaming_.texcoordMinU_ = mesh.TexcoordMin().x;
				instanceData.streaming_.texcoordMinV_ = mesh.TexcoordMin().y;
				instanceData.streaming_.texcoordExtent_ = mesh.TexcoordExtent();

				instanceData.streaming_.lodError_ = 0.0f;
				instanceData.streaming_.lodErrorNext_ = FLT_MAX;

				instanceData.shading_.doubleSided_ = 1;
				instanceData.shading_.blend_ = 0;
				instanceData.shading_.selected_ = 0;

				instances_.push_back(instanceData);
			}
		}
	}

	void AvatarRenderer::Upload()
	{
		if (uploaded_)
		{
			return;
		}
		uploaded_ = true;

		shaderResourceIndices_.model_.instanceIndex_ = instanceBuffer_->Index();
		shaderResourceIndices_.model_.boneMatrixIndex_ = boneBuffer_->Index();
		shaderResourceIndices_.model_.previousBoneMatrixIndex_ = boneBuffer_->Index();

		if (!instances_.empty())
		{
			instanceBuffer_->Update(instances_.data(), static_cast<Uint>(instances_.size()));
		}
		if (!boneMatrices_.empty())
		{
			boneBuffer_->Update(boneMatrices_.data(), static_cast<Uint>(boneMatrices_.size()));
		}

		if (D3D12Check::GetLevel() != D3D12Level::D12_2)
		{
			modelCullingBuffer_.Reserve(static_cast<Uint>(instances_.size()) * maxMeshletsPerDispatch_);
		}
	}

	void AvatarRenderer::Begin(D3D12CommandList* cmdList)
	{
		frameBuffer_->Begin(cmdList);
		frameBuffer_->Clear(cmdList, 0.45f, 0.65f, 0.9f, 1.0f);
	}

	void AvatarRenderer::Draw(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const SceneConstantBuffer& scene)
	{
		sceneSystem_->Upload(scene);

		constantIndices_.sceneIndex_ = sceneSystem_->GetIndex();
		constantIndicesBuffer_->Update(constantIndices_);
		shaderResourceIndicesBuffer_->Update(shaderResourceIndices_);

		if (instances_.empty())
		{
			return;
		}

		D3D12_GPU_VIRTUAL_ADDRESS constantAddr = constantIndicesBuffer_->Address();
		D3D12_GPU_VIRTUAL_ADDRESS shaderResourceAddr = shaderResourceIndicesBuffer_->Address();

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantAddr);
		cmd->SetGraphicsRootConstantBufferView(0, shaderResourceAddr);

		if (D3D12Check::GetLevel() == D3D12Level::D12_2)
		{
			cmd->SetPipelineState(modelShader_.GetPipelineStateAvatarPreview());
			cmd->DispatchMesh(static_cast<Uint>(instances_.size()), 1, 1);
			ProfilerStats::AddDrawCall();
		}
		else
		{
			modelCullingBuffer_.Begin(cmd);

			cmd->SetComputeRootSignature(modelShader_.GetRootSignature());
			cmd->SetComputeRootConstantBufferView(2, constantAddr);
			cmd->SetComputeRootConstantBufferView(0, shaderResourceAddr);
			Uint cullingIndex = modelCullingBuffer_.GetConstantBufferIndex();
			cmd->SetComputeRoot32BitConstants(3, 1, &cullingIndex, 0);
			cmd->SetPipelineState(modelShader_.GetPipelineStateModelCulling());
			cmd->Dispatch(static_cast<Uint>(instances_.size()), 1, 1);

			modelCullingBuffer_.Barrier(cmd);

			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			Uint singleSidedIndex = modelCullingBuffer_.GetSingleSidedShaderResourceViewIndex();
			Uint doubleSidedIndex = modelCullingBuffer_.GetDoubleSidedShaderResourceViewIndex();

			cmd->SetGraphicsRoot32BitConstants(3, 1, &singleSidedIndex, 0);
			cmd->SetPipelineState(modelShader_.GetPipelineStateAvatarPreview());
			cmd->ExecuteIndirect(modelCullingBuffer_.GetCommandSignature(), 1, modelCullingBuffer_.GetArgumentBuffer(), 0, nullptr, 0);
			ProfilerStats::AddDrawCall();

			cmd->SetGraphicsRoot32BitConstants(3, 1, &doubleSidedIndex, 0);
			cmd->SetPipelineState(modelShader_.GetPipelineStateAvatarPreviewDoubleSided());
			cmd->ExecuteIndirect(modelCullingBuffer_.GetCommandSignature(), 1, modelCullingBuffer_.GetArgumentBuffer(), sizeof(D3D12_DRAW_ARGUMENTS), nullptr, 0);
			ProfilerStats::AddDrawCall();

			modelCullingBuffer_.End(cmd);
		}
	}

	void AvatarRenderer::End(D3D12CommandList* cmdList)
	{
		frameBuffer_->End(cmdList);
	}

	void AvatarRenderer::RegisterImGuiShaderResourceView(ID3D12Device* device, DescriptorHeap* imguiHeap)
	{
		Bool alreadyRegistered = imguiHeap_ != nullptr;
		imguiHeap_ = imguiHeap;

		if (!alreadyRegistered)
		{
			imguiShaderResourceViewIndex_ = imguiHeap->AllocateIndex();
		}

		D3D12_RESOURCE_DESC desc = frameBuffer_->ColorResource()->GetDesc();

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription{};
		shaderResourceViewDescription.Format = desc.Format;
		shaderResourceViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDescription.Texture2D.MipLevels = 1;

		device->CreateShaderResourceView(frameBuffer_->ColorResource(), &shaderResourceViewDescription, imguiHeap->CPUHandle(imguiShaderResourceViewIndex_));
	}

	D3D12_GPU_DESCRIPTOR_HANDLE AvatarRenderer::ImGuiGPUHandle()const
	{
		return imguiHeap_->GPUHandle(imguiShaderResourceViewIndex_);
	}
}
