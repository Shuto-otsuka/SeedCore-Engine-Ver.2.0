#include <GraphicsEngine/Renderer/ImageRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/Texture/Image.h>
#include <GraphicsEngine/Texture/Texture.h>
#include <GraphicsEngine/Texture/ImageResource.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>

namespace SeedCore
{
	ImageRenderer::ImageRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : imageShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void ImageRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem)
	{
		bindlessHeap_ = bindlessHeap;
		maxCount_ = 65536;

		imageShader_.Create(shaderCache, device);

		indicesSystem_ = &indicesSystem;

		spriteBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ImageSpriteInstance>>(device, bindlessHeap, maxCount_);
		billboardBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ImageBillboardInstance>>(device, bindlessHeap, maxCount_);

		indicesSystem.SetImageSpriteIndex(spriteBuffer_->Index());
		indicesSystem.SetImageBillboardIndex(billboardBuffer_->Index());
	}

	void ImageRenderer::Gather(LoaderSystem& loader, ImageResource& resource, World& world, Entity selectedEntity)
	{
		spriteInstances_.clear();
		billboardInstances_.clear();
		hasSelectedSpriteInstance_ = false;
		hasSelectedBillboardInstance_ = false;

		Query<Read<Active>, Read<Image>> query(world);
		query.ForEach([&](EntityID entityID, const Active& active, const Image& image)
			{
				if (!active.active_)
				{
					return;
				}

				/// [EN] Read the parent-composed world transform from
				///      TransformSystem (Actor::GetWorldMatrix()) instead of
				///      the actor's own local Position/Rotation/Scale, so
				///      parented images follow their parent.
				/// [JP] アクター自身のローカル Position/Rotation/Scale ではなく
				///      TransformSystem(Actor::GetWorldMatrix())が計算した
				///      親合成済みのワールド変換を読む。
				Actor* actor = world.GetActor(entityID);
				if (!actor)
				{
					return;
				}

				Matrix worldMatrix = actor->GetWorldMatrix();
				Vector3 worldScale;
				Quaternion worldRotation;
				Vector3 worldTranslation;
				worldMatrix.Decompose(worldScale, worldRotation, worldTranslation);

				Handle<Texture> textureHandle = resource.GetHandle(image.textureID_);
				if (textureHandle.empty())
				{
					return;
				}

				Texture* texture = resource.Resolve(loader, bindlessHeap_, textureHandle, streamingFrame_);
				if (!texture)
				{
					return;
				}

				Uint32 textureIndex = texture->textureIndex_;

				Vector2 textureSize = image.textureSize_;
				if (textureSize.x == 0.0f && textureSize.y == 0.0f && texture->resource_)
				{
					D3D12_RESOURCE_DESC desc = texture->resource_->GetDesc();
					textureSize = Vector2(static_cast<Float>(desc.Width), static_cast<Float>(desc.Height));
				}

				Uint selected = (selectedEntity.Exists() && actor->GetEntity() == selectedEntity) ? 1 : 0;
				Uint motionType = static_cast<Uint>(image.motionType_);

				if (image.viewType_ == Image::ViewType::Sprite)
				{
					hasSelectedSpriteInstance_ = hasSelectedSpriteInstance_ || selected != 0;

					Vector2 position = Vector2(worldTranslation.x, worldTranslation.y);
					Float rotationAngle = worldRotation.ToEuler().x;
					Vector2 scale = Vector2(worldScale.x, worldScale.y);

					ImageSpriteInstance instance{};
					instance.position_ = position;
					instance.rotation_ = rotationAngle;
					instance.scale_ = scale;
					instance.textureSize_ = textureSize;
					instance.texturePosition_ = Vector2(image.texturePosition_.x, image.texturePosition_.y);
					instance.pivot_ = Vector2(image.pivot_.x, image.pivot_.y);
					instance.color_ = image.color_;
					instance.textureIndex_ = textureIndex;
					instance.scrollSpeed_ = image.scrollSpeed_;
					instance.scrollDirection_ = image.scrollDirection_;
					instance.motionType_ = motionType;
					instance.selected_ = selected;
					spriteInstances_.push_back(instance);
				}
				else
				{
					hasSelectedBillboardInstance_ = hasSelectedBillboardInstance_ || selected != 0;

					ImageBillboardInstance instance{};
					instance.position_ = worldTranslation;
					instance.rotation_ = worldRotation.ToEuler();
					instance.scale_ = Vector2(worldScale.x, worldScale.y);
					instance.textureSize_ = textureSize;
					instance.texturePosition_ = Vector2(image.texturePosition_.x, image.texturePosition_.y);
					instance.pivot_ = Vector2(image.pivot_.x, image.pivot_.y);
					instance.color_ = image.color_;
					instance.textureIndex_ = textureIndex;
					instance.scrollSpeed_ = image.scrollSpeed_;
					instance.scrollDirection_ = image.scrollDirection_;
					instance.motionType_ = motionType;
					instance.faceCamera_ = image.faceCamera_ ? 1 : 0;
					instance.selected_ = selected;
					billboardInstances_.push_back(instance);
				}
			});

		resource.EvictBudget(loader, bindlessHeap_, streamingFrame_);
		streamingFrame_++;
	}

	void ImageRenderer::Upload()
	{
		indicesSystem_->SetImageSpriteIndex(spriteBuffer_->Index());
		indicesSystem_->SetImageBillboardIndex(billboardBuffer_->Index());

		if (!spriteInstances_.empty())
		{
			spriteBuffer_->Update(spriteInstances_.data(), static_cast<Uint>(spriteInstances_.size()));
		}

		if (!billboardInstances_.empty())
		{
			billboardBuffer_->Update(billboardInstances_.data(), static_cast<Uint>(billboardInstances_.size()));
		}
	}

	void ImageRenderer::DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (spriteInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(imageShader_.GetRootSignature());

		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(imageShader_.GetPipelineStateSprite());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(spriteInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void ImageRenderer::DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (billboardInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(imageShader_.GetRootSignature());

		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(imageShader_.GetPipelineStateBillboard());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(billboardInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void ImageRenderer::DrawSelectionMaskSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (!hasSelectedSpriteInstance_)
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(imageShader_.GetRootSignature());
		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(imageShader_.GetPipelineStateSelectionMaskSprite());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(spriteInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void ImageRenderer::DrawSelectionMaskBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (!hasSelectedBillboardInstance_)
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(imageShader_.GetRootSignature());
		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(imageShader_.GetPipelineStateSelectionMaskBillboard());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(billboardInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}
}
