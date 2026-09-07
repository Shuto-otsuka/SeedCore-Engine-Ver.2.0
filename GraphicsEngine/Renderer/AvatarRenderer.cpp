#include <GraphicsEngine/Renderer/AvatarRenderer.h>
#include <GraphicsEngine/Avatar/AvatarMesh.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterEvaluator.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>

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

		instanceBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ModelInstanceData>>(device, bindlessHeap, maxInstanceCount_);
		boneBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Matrix>>(device, bindlessHeap, maxBoneCount_);

		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		frameBuffer_ = MakePtr<FrameBuffer>(device, &renderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &depthStencilViewHeap_, 0.45f, 0.65f, 0.9f, 1.0f);

		sceneSystem_ = MakePtr<SceneSystem>(device, bindlessHeap);

		constantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, bindlessHeap);
		structuredIndicesBuffer_ = MakePtr<ConstantBuffer<StructuredIndices>>(device, bindlessHeap);
	}

	void AvatarRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		frameBuffer_->Resize(device, bindlessHeap, width, height);
	}

	void AvatarRenderer::Gather(const AvatarMesh& mesh, const HumanCharacterEvaluator& evaluator, const Matrix& worldMatrix)
	{
		instances_.clear();
		boneMatrices_.clear();
		uploaded_ = false;

		if (!mesh.IsCreated() || mesh.MeshletCount() == 0)
		{
			return;
		}

		Uint32 boneCount = static_cast<Uint32>(evaluator.JointHeads().size());
		if (boneCount == 0 || boneCount > maxBoneCount_)
		{
			return;
		}
		boneMatrices_.assign(boneCount, Matrix::Identity);

		Matrix inverseTransposeWorld = worldMatrix.Invert().Transpose();

		Uint32 meshletCount = mesh.MeshletCount();
		for (Uint32 meshletOffset = 0; meshletOffset < meshletCount; meshletOffset += maxMeshletsPerDispatch_)
		{
			Uint32 count = Min(maxMeshletsPerDispatch_, meshletCount - meshletOffset);

			ModelInstanceData instanceData{};
			instanceData.world_ = worldMatrix;
			instanceData.inverseTransposeWorld_ = inverseTransposeWorld;
			instanceData.previousWorld_ = worldMatrix;

			instanceData.baseColor_ = Color(0.78f, 0.76f, 0.74f, 1.0f);
			instanceData.metallic_ = 0.0f;
			instanceData.roughness_ = 0.85f;
			instanceData.alphaCutoff_ = 0.0f;
			instanceData.shadingModel_ = static_cast<Uint>(ShadingModel::Lambert);
			instanceData.unlit_ = 0.0f;

			instanceData.baseColorTextureIndex_ = 0xFFFFFFFF;
			instanceData.normalTextureIndex_ = 0xFFFFFFFF;
			instanceData.metallicRoughnessTextureIndex_ = 0xFFFFFFFF;
			instanceData.emissiveTextureIndex_ = 0xFFFFFFFF;
			instanceData.occlusionTextureIndex_ = 0xFFFFFFFF;

			instanceData.vertexBufferIndex_ = mesh.VertexBufferIndex();
			instanceData.meshletBufferIndex_ = mesh.MeshletBufferIndex();
			instanceData.meshletBoundBufferIndex_ = mesh.MeshletBoundBufferIndex();
			instanceData.vertexIndicesBufferIndex_ = mesh.VertexIndicesBufferIndex();
			instanceData.primitiveIndicesBufferIndex_ = mesh.PrimitiveIndicesBufferIndex();
			instanceData.skinVertexBufferIndex_ = 0xFFFFFFFF;

			instanceData.meshletOffset_ = meshletOffset;
			instanceData.meshletCount_ = count;

			instanceData.skinIndex_ = 0xFFFFFFFF;
			instanceData.boneOffset_ = 0;

			instanceData.positionMin_ = mesh.PositionMin();
			instanceData.positionExtent_ = mesh.PositionExtent();
			instanceData.texcoordMinU_ = mesh.TexcoordMin().x;
			instanceData.texcoordMinV_ = mesh.TexcoordMin().y;
			instanceData.texcoordExtent_ = mesh.TexcoordExtent();

			instanceData.lodError_ = 0.0f;
			instanceData.lodErrorNext_ = FLT_MAX;

			instanceData.doubleSided_ = 1;
			instanceData.blend_ = 0;
			instanceData.selected_ = 0;

			instances_.push_back(instanceData);
		}
	}

	void AvatarRenderer::Upload()
	{
		if (uploaded_)
		{
			return;
		}
		uploaded_ = true;

		structuredIndices_.model_.instanceIndex_ = instanceBuffer_->Index();
		structuredIndices_.model_.boneMatrixIndex_ = boneBuffer_->Index();

		if (!instances_.empty())
		{
			instanceBuffer_->Update(instances_.data(), static_cast<Uint>(instances_.size()));
		}
		if (!boneMatrices_.empty())
		{
			boneBuffer_->Update(boneMatrices_.data(), static_cast<Uint>(boneMatrices_.size()));
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
		structuredIndicesBuffer_->Update(structuredIndices_);

		if (instances_.empty())
		{
			return;
		}

		D3D12_GPU_VIRTUAL_ADDRESS constantAddr = constantIndicesBuffer_->Address();
		D3D12_GPU_VIRTUAL_ADDRESS structuredAddr = structuredIndicesBuffer_->Address();

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantAddr);
		cmd->SetGraphicsRootConstantBufferView(3, structuredAddr);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(modelShader_.GetPipelineStateAvatarPreview());
		cmd->DispatchMesh(static_cast<Uint>(instances_.size()), 1, 1);
		ProfilerStats::AddDrawCall();
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
