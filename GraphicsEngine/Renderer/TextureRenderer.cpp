#include <GraphicsEngine/Renderer/TextureRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/Texture/Image.h>
#include <GraphicsEngine/Texture/Texture.h>
#include <GraphicsEngine/Texture/TextureResource.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/Context/D3D12Check.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <FoundationEngine/World/ECS/Query/Query.h>
#include <FoundationEngine/World/ECS/Component/Active.h>
#include <FoundationEngine/World/ECS/Component/Bounds.h>
#include <FoundationEngine/World/ECS/Component/ComponentRegistry.h>

namespace SeedCore
{
	TextureRenderer::TextureRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : imageShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void TextureRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, ShaderResourceIndicesSystem& shaderResourceIndicesSystem)
	{
		bindlessHeap_ = bindlessHeap;
		maxCount_ = 65536;

		imageShader_.Create(shaderCache, device);

		shaderResourceIndicesSystem_ = &shaderResourceIndicesSystem;

		spriteBuffer_ = MakePtr<ReadOnlyStructuredBuffer<TextureSpriteStructuredBuffer>>(device, bindlessHeap, maxCount_);
		billboardBuffer_ = MakePtr<ReadOnlyStructuredBuffer<TextureBillboardStructuredBuffer>>(device, bindlessHeap, maxCount_);

		shaderResourceIndicesSystem.SetTextureSpriteIndex(spriteBuffer_->Index());
		shaderResourceIndicesSystem.SetTextureBillboardIndex(billboardBuffer_->Index());
	}

	void TextureRenderer::Gather(LoaderSystem& loader, TextureResource& resource, World& world, Vector2 nativeScreenSize, std::span<const Entity> selectedEntities)
	{
		spriteInstances_.clear();
		billboardInstances_.clear();
		hasSelectedSpriteInstance_ = false;
		hasSelectedBillboardInstance_ = false;

		Float spriteReferenceScale = (ScResolution::SC_CANVAS.Height > 0.0f) ? (nativeScreenSize.y / ScResolution::SC_CANVAS.Height) : 1.0f;

		/// [EN] Cached once so each Image actor's Bounds can be synced to its texture rect below (mirrors ModelRenderer's Mesh -> Bounds sync), giving 2D elements a viewport-pickable box.
		/// [JP] 各 Image アクターの Bounds を下でテクスチャ矩形に同期できるよう一度だけキャッシュする(ModelRenderer の Mesh -> Bounds 同期と同じ)。これで 2D 要素にビューポートでピック可能なボックスが付く。
		ComponentID boundsComponentID = ComponentRegistry::GetComponentID<Bounds>();

		Query<Read<Active>, Read<Image>> query(world);
		query.ForEach([&](EntityID entityID, const Active& active, const Image& image)
			{
				if (!active.active_)
				{
					return;
				}

				/// [EN] Read the parent-composed world transform from
				///      TransformSystem (Actor::WorldMatrix()) instead of
				///      the actor's own local Position/Rotation/Scale, so
				///      parented images follow their parent.
				/// [JP] アクター自身のローカル Position/Rotation/Scale ではなく
				///      TransformSystem(Actor::WorldMatrix())が計算した
				///      親合成済みのワールド変換を読む。
				Actor actor = world.GetActor(entityID);
				if (!actor)
				{
					return;
				}

				Matrix worldMatrix = actor.WorldMatrix();
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

				Uint32 textureIndex = texture->ShaderResourceViewIndex();

				Vector2 textureSize = image.textureSize_;
				if (textureSize.x == 0.0f && textureSize.y == 0.0f && texture->Resource())
				{
					D3D12_RESOURCE_DESC desc = texture->Resource()->GetDesc();
					textureSize = Vector2(static_cast<Float>(desc.Width), static_cast<Float>(desc.Height));
				}

				/// [EN] Sync this actor's Bounds (every actor has one) to the sprite's local-space rect: half-extents = textureSize/2, centre shifted by the pivot the same way TextureBillboardMS.hlsl shifts the quad (0.5 - pivotNorm on X, pivotNorm - 0.5 on Y). The actor's world matrix then rotates/scales it, so ViewportPicking gets a correct OBB for the drawn sprite.
				/// [JP] このアクターの Bounds(全アクターが持つ)をスプライトのローカル空間矩形へ同期する: 半径 = textureSize/2、中心は TextureBillboardMS.hlsl がクアッドをずらすのと同じ形(X は 0.5 - pivotNorm、Y は pivotNorm - 0.5)で pivot ぶんずらす。アクターのワールド行列がこれを回転/スケールするので、ViewportPicking は描画済みスプライトの正しい OBB を得る。
				if (boundsComponentID)
				{
					void* boundsRaw = world.GetComponent(entityID, boundsComponentID);
					if (boundsRaw)
					{
						Vector2 pivotNorm = (textureSize.x > 0.0f && textureSize.y > 0.0f) ? Vector2(image.pivot_.x / textureSize.x, image.pivot_.y / textureSize.y) : Vector2(0.5f, 0.5f);
						Bounds* bounds = static_cast<Bounds*>(boundsRaw);
						bounds->center_ = Vector3((0.5f - pivotNorm.x) * textureSize.x, (pivotNorm.y - 0.5f) * textureSize.y, 0.0f);
						bounds->extent_ = Vector3(textureSize.x * 0.5f, textureSize.y * 0.5f, 1.0f);
					}
				}

				Uint selected = std::ranges::find(selectedEntities, actor.GetEntity()) != selectedEntities.end() ? 1 : 0;
				Uint motionType = static_cast<Uint>(image.motionType_);

				if (image.viewType_ == Image::ViewType::Sprite)
				{
					hasSelectedSpriteInstance_ = hasSelectedSpriteInstance_ || selected != 0;

					Vector2 position = Vector2(worldTranslation.x, worldTranslation.y);
					Float rotationAngle = worldRotation.ToEuler().x;
					Vector2 scale = Vector2(worldScale.x, worldScale.y);

					TextureSpriteStructuredBuffer instance{};
					instance.position_ = position * spriteReferenceScale;
					instance.rotation_ = rotationAngle;
					instance.scale_ = scale * spriteReferenceScale;
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

					hasSelectedBillboardInstance_ = hasSelectedBillboardInstance_ || selected != 0;

					TextureBillboardStructuredBuffer canvasInstance{};
					canvasInstance.position_ = Vector3(100000.0f + position.x, 100000.0f + (ScResolution::SC_CANVAS.Height - position.y), 100000.0f);
					canvasInstance.rotation_ = Vector3(0.0f, 0.0f, rotationAngle);
					canvasInstance.scale_ = Vector2(scale.x * textureSize.x, scale.y * textureSize.y);
					canvasInstance.textureSize_ = textureSize;
					canvasInstance.texturePosition_ = Vector2(image.texturePosition_.x, image.texturePosition_.y);
					canvasInstance.pivot_ = Vector2(image.pivot_.x, image.pivot_.y);
					canvasInstance.color_ = image.color_;
					canvasInstance.textureIndex_ = textureIndex;
					canvasInstance.scrollSpeed_ = image.scrollSpeed_;
					canvasInstance.scrollDirection_ = image.scrollDirection_;
					canvasInstance.motionType_ = motionType;
					canvasInstance.faceCamera_ = 1;
					canvasInstance.selected_ = selected;
					billboardInstances_.push_back(canvasInstance);
				}
				else
				{
					hasSelectedBillboardInstance_ = hasSelectedBillboardInstance_ || selected != 0;

					TextureBillboardStructuredBuffer instance{};
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

	void TextureRenderer::Upload()
	{
		shaderResourceIndicesSystem_->SetTextureSpriteIndex(spriteBuffer_->Index());
		shaderResourceIndicesSystem_->SetTextureBillboardIndex(billboardBuffer_->Index());

		if (!spriteInstances_.empty())
		{
			spriteBuffer_->Update(spriteInstances_.data(), static_cast<Uint>(spriteInstances_.size()));
		}

		if (!billboardInstances_.empty())
		{
			billboardBuffer_->Update(billboardInstances_.data(), static_cast<Uint>(billboardInstances_.size()));
		}
	}

	void TextureRenderer::DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses)
	{
		if (spriteInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(imageShader_.GetRootSignature());

		RootSignature::BindGraphics(cmdList, addresses);

		cmdList->SetPipelineState(imageShader_.GetPipelineStateSprite());

		if (D3D12Check::GetLevel() == D3D12Level::D12_2)
		{
			Uint groupCount = (static_cast<Uint>(spriteInstances_.size()) + 31) / 32;
			cmdList->DispatchMesh(groupCount, 1, 1);
		}
		else
		{
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->DrawInstanced(6, static_cast<Uint>(spriteInstances_.size()), 0, 0);
		}
		ProfilerStats::AddDrawCall();
	}

	void TextureRenderer::DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses)
	{
		if (billboardInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(imageShader_.GetRootSignature());

		RootSignature::BindGraphics(cmdList, addresses);

		cmdList->SetPipelineState(imageShader_.GetPipelineStateBillboard());

		if (D3D12Check::GetLevel() == D3D12Level::D12_2)
		{
			Uint groupCount = (static_cast<Uint>(billboardInstances_.size()) + 31) / 32;
			cmdList->DispatchMesh(groupCount, 1, 1);
		}
		else
		{
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->DrawInstanced(6, static_cast<Uint>(billboardInstances_.size()), 0, 0);
		}
		ProfilerStats::AddDrawCall();
	}

	void TextureRenderer::DrawSilhouetteSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses)
	{
		if (!hasSelectedSpriteInstance_)
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(imageShader_.GetRootSignature());
		RootSignature::BindGraphics(cmdList, addresses);

		cmdList->SetPipelineState(imageShader_.GetPipelineStateSilhouetteSprite());

		if (D3D12Check::GetLevel() == D3D12Level::D12_2)
		{
			Uint groupCount = (static_cast<Uint>(spriteInstances_.size()) + 31) / 32;
			cmdList->DispatchMesh(groupCount, 1, 1);
		}
		else
		{
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->DrawInstanced(6, static_cast<Uint>(spriteInstances_.size()), 0, 0);
		}
		ProfilerStats::AddDrawCall();
	}

	void TextureRenderer::DrawSilhouetteBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses)
	{
		if (!hasSelectedBillboardInstance_)
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(imageShader_.GetRootSignature());
		RootSignature::BindGraphics(cmdList, addresses);

		cmdList->SetPipelineState(imageShader_.GetPipelineStateSilhouetteBillboard());

		if (D3D12Check::GetLevel() == D3D12Level::D12_2)
		{
			Uint groupCount = (static_cast<Uint>(billboardInstances_.size()) + 31) / 32;
			cmdList->DispatchMesh(groupCount, 1, 1);
		}
		else
		{
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->DrawInstanced(6, static_cast<Uint>(billboardInstances_.size()), 0, 0);
		}
		ProfilerStats::AddDrawCall();
	}
}
