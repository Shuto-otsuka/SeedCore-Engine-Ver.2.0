#include <GraphicsEngine/Renderer/FontRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/Font/Text.h>
#include <GraphicsEngine/Font/Font.h>
#include <GraphicsEngine/Font/FontResource.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/ECS/Component/Bounds.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	FontRenderer::FontRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : fontShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void FontRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem)
	{
		bindlessHeap_ = bindlessHeap;
		maxCount_ = 65536;

		fontShader_.Create(shaderCache, device);

		indicesSystem_ = &indicesSystem;

		spriteBuffer_ = MakePtr<ReadOnlyStructuredBuffer<FontSpriteInstance>>(device, bindlessHeap, maxCount_);
		billboardBuffer_ = MakePtr<ReadOnlyStructuredBuffer<FontBillboardInstance>>(device, bindlessHeap, maxCount_);

		
		

		indicesSystem.SetFontSpriteIndex(spriteBuffer_->Index());
		indicesSystem.SetFontBillboardIndex(billboardBuffer_->Index());


	}

	void FontRenderer::Gather(FontResource& fontResource, World& world, Vector2 nativeScreenSize, Entity selectedEntity)
	{
		spriteInstances_.clear();
		billboardInstances_.clear();

		Float spriteReferenceScale = (ScResolution::SC_HD.Height > 0.0f) ? (nativeScreenSize.y / ScResolution::SC_HD.Height) : 1.0f;

		/// [EN] Cached once so each sprite-view Text actor's Bounds can be synced to its measured text box below (mirrors ImageRenderer's Image -> Bounds sync), giving canvas text a pickable box in CanvasViewPanel.
		/// [JP] 各スプライトビュー Text アクターの Bounds を下で計測したテキストボックスへ同期できるよう一度だけキャッシュする(ImageRenderer の Image -> Bounds 同期と同じ)。これでキャンバステキストが CanvasViewPanel でピック可能なボックスを持つ。
		ComponentID boundsComponentID = ComponentRegistry::GetComponentID<Bounds>();

		Query<Read<Active>, Read<Text>> query(world);
		query.ForEach([&](EntityID entityID, const Active& active, const Text& text)
			{
				if (!active.active_)
				{
					return;
				}

				if (text.text_.str().empty())
				{
					return;
				}

				/// [EN] Read the parent-composed world transform from
				///      TransformSystem (Actor::GetWorldMatrix()), same as
				///      ImageRenderer, so parented text follows its parent.
				///      rotationEuler here is TransformSystem's own
				///      yaw/pitch/roll decomposition of the composed
				///      rotation, matching CreateFromYawPitchRoll's axes.
				/// [JP] ImageRenderer と同様、アクター自身のローカル値ではなく
				///      TransformSystem(Actor::GetWorldMatrix())の親合成済み
				///      ワールド変換を読む。rotationEuler は合成後回転を
				///      CreateFromYawPitchRoll と同じ軸(yaw/pitch/roll)で
				///      分解したもの。
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

				Vector3 position = worldTranslation;
				Vector2 scale = Vector2(worldScale.x, worldScale.y);
				Vector3 rotationEuler = worldRotation.ToEuler(); // (pitch, yaw, roll)

				Uint selected = (selectedEntity.Exists() && actor->GetEntity() == selectedEntity) ? 1 : 0;

				Font* font = fontResource.Find(text.fontID_);
				if (!font)
				{
					return;
				}

				font->Shape(text.text_.str());

				if (!font->HasAtlasTexture())
				{
					return;
				}

				msdfgen::BitmapConstRef<msdf_atlas::byte, 4> bitmap = font->GetAtlasBitmap();
				if (bitmap.width <= 0 || bitmap.height <= 0)
				{
					return;
				}

				const Float scaleFactor = text.fontSize_ / font->GetFontSize();
				const Float lineAdvance = font->GetLineHeight() * scaleFactor * text.lineSpacing_;
				const Vector2 unitRange = Vector2(font->GetPixelRange() / static_cast<Float>(bitmap.width), font->GetPixelRange() / static_cast<Float>(bitmap.height));
				const Float inverseWidth = 1.0f / static_cast<Float>(bitmap.width);
				const Float inverseHeight = 1.0f / static_cast<Float>(bitmap.height);

				const Bool isSprite = (text.viewType_ == Text::ViewType::Sprite);

				/// [EN] Canvas text box in unscaled glyph space, relative to the pivot, accumulated over every drawn glyph (same convention as ImageRenderer's Bounds: the actor's world scale is applied by the picker, not stored). Stays inverted (min > max) when the text draws nothing.
				/// [JP] pivot 基準・未スケールのグリフ空間でのキャンバステキストボックス。描画される全グリフにわたって蓄積する(ImageRenderer の Bounds と同じ規約: アクターのワールドスケールはピッカー側で掛ける、保存しない)。何も描画しないときは反転(min > max)のまま。
				Float textBoxMinX = FLT_MAX;
				Float textBoxMaxX = -FLT_MAX;
				Float textBoxMinY = FLT_MAX;
				Float textBoxMaxY = -FLT_MAX;

				Float lineTop = 0.0f;
				std::istringstream stream(text.text_.str());
				std::string line;
				while (std::getline(stream, line))
				{
					const Float baseline = lineTop + font->GetAscender() * scaleFactor;

					if (!line.empty())
					{
						Vector2 pen = { 0.0f, 0.0f };
						for (const ShapedGlyph& shaped : font->Shape(line))
						{
							const GlyphAtlasEntry* entry = font->FindGlyph(shaped.glyphIndex_);
							if (entry && !entry->isWhitespace_)
							{
								const Float glyphLeft = pen.x + shaped.offset_.x * scaleFactor + entry->planeLeft_ * scaleFactor;
								const Float glyphTop = baseline - shaped.offset_.y * scaleFactor - entry->planeTop_ * scaleFactor;
								const Float glyphWidth = (entry->planeRight_ - entry->planeLeft_) * scaleFactor;
								const Float glyphHeight = (entry->planeTop_ - entry->planeBottom_) * scaleFactor;

								const Vector2 uvMin = Vector2(entry->atlasLeft_ * inverseWidth, entry->atlasTop_ * inverseHeight);
								const Vector2 uvMax = Vector2(entry->atlasRight_ * inverseWidth, entry->atlasBottom_ * inverseHeight);

								if (isSprite)
								{
									/// [EN] Grow the canvas text box by this glyph (unscaled, pivot-relative - matches canvasInstance.localPosition_/localSize_ divided by scale).
									/// [JP] このグリフぶんキャンバステキストボックスを広げる(未スケール・pivot 基準 - canvasInstance.localPosition_/localSize_ をスケールで割ったものと一致)。
									textBoxMinX = Min(textBoxMinX, glyphLeft - text.pivot_.x);
									textBoxMaxX = Max(textBoxMaxX, glyphLeft - text.pivot_.x + glyphWidth);
									textBoxMinY = Min(textBoxMinY, text.pivot_.y - glyphTop - glyphHeight);
									textBoxMaxY = Max(textBoxMaxY, text.pivot_.y - glyphTop);

									FontSpriteInstance instance{};
									instance.size_ = Vector2(glyphWidth * scale.x, glyphHeight * scale.y);
									instance.uvMin_ = uvMin;
									instance.uvMax_ = uvMax;
									instance.color_ = text.color_;
									instance.outlineColor_ = text.outlineColor_;
									instance.glowColor_ = text.glowColor_;
									instance.textureIndex_ = font->GetAtlasTextureIndex();
									instance.outlineWidth_ = text.outlineWidth_;
									instance.glowPower_ = text.glowPower_;
									instance.unitRange_ = unitRange;

									const Vector2 basePosition = Vector2(position.x * spriteReferenceScale - text.pivot_.x + glyphLeft * scale.x, position.y * spriteReferenceScale - text.pivot_.y + glyphTop * scale.y);

									if (text.shadowEnable_)
									{
										FontSpriteInstance shadow = instance;
										shadow.position_ = basePosition + Vector2(text.shadowOffset_.x * scale.x, text.shadowOffset_.y * scale.y);
										shadow.color_ = text.shadowColor_;
										shadow.outlineColor_ = text.shadowColor_;
										shadow.glowPower_ = 0.0f;
										shadow.selected_ = selected;
										spriteInstances_.push_back(shadow);
									}

									instance.position_ = basePosition;
									instance.selected_ = selected;
									spriteInstances_.push_back(instance);

									FontBillboardInstance canvasInstance{};
									canvasInstance.position_ = Vector3(100000.0f + position.x, 100000.0f + (ScResolution::SC_HD.Height - position.y), 100000.0f);
									canvasInstance.rotation_ = Vector3::Zero;
									canvasInstance.localPosition_ = Vector2((glyphLeft - text.pivot_.x) * scale.x, (text.pivot_.y - glyphTop - glyphHeight) * scale.y);
									canvasInstance.localSize_ = Vector2(glyphWidth * scale.x, glyphHeight * scale.y);
									canvasInstance.uvMin_ = uvMin;
									canvasInstance.uvMax_ = uvMax;
									canvasInstance.color_ = text.color_;
									canvasInstance.outlineColor_ = text.outlineColor_;
									canvasInstance.glowColor_ = text.glowColor_;
									canvasInstance.textureIndex_ = font->GetAtlasTextureIndex();
									canvasInstance.outlineWidth_ = text.outlineWidth_;
									canvasInstance.glowPower_ = text.glowPower_;
									canvasInstance.unitRange_ = unitRange;
									canvasInstance.faceCamera_ = 1;
									canvasInstance.selected_ = selected;
									billboardInstances_.push_back(canvasInstance);
								}
								else
								{
									FontBillboardInstance instance{};
									instance.position_ = position;
									instance.rotation_ = rotationEuler;
									instance.uvMin_ = uvMin;
									instance.uvMax_ = uvMax;
									instance.color_ = text.color_;
									instance.outlineColor_ = text.outlineColor_;
									instance.glowColor_ = text.glowColor_;
									instance.textureIndex_ = font->GetAtlasTextureIndex();
									instance.outlineWidth_ = text.outlineWidth_;
									instance.glowPower_ = text.glowPower_;
									instance.unitRange_ = unitRange;
									instance.faceCamera_ = text.faceCamera_ ? 1 : 0;

									const Float pixelToUnitX = 0.01f * scale.x;
									const Float pixelToUnitY = 0.01f * scale.y;
									const Vector2 baseLocal = Vector2((glyphLeft - text.pivot_.x) * pixelToUnitX, (text.pivot_.y - glyphTop - glyphHeight) * pixelToUnitY);
									instance.localSize_ = Vector2(glyphWidth * pixelToUnitX, glyphHeight * pixelToUnitY);

									if (text.shadowEnable_)
									{
										FontBillboardInstance shadow = instance;
										shadow.localPosition_ = baseLocal + Vector2(text.shadowOffset_.x * pixelToUnitX, -text.shadowOffset_.y * pixelToUnitY);
										shadow.color_ = text.shadowColor_;
										shadow.outlineColor_ = text.shadowColor_;
										shadow.glowPower_ = 0.0f;

										const Float rx = instance.rotation_.x;
										const Float ry = instance.rotation_.y;
										const Float rz = instance.rotation_.z;
										const Vector3 normal = Vector3(
											std::cos(rx) * std::sin(ry) * std::cos(rz) + std::sin(rx) * std::sin(rz),
											std::cos(rx) * std::sin(ry) * std::sin(rz) - std::sin(rx) * std::cos(rz),
											std::cos(rx) * std::cos(ry)
										);
										shadow.position_ = instance.position_ + normal * 0.001f;
										shadow.selected_ = selected;

										billboardInstances_.push_back(shadow);
									}

									instance.localPosition_ = baseLocal;
									instance.selected_ = selected;
									billboardInstances_.push_back(instance);
								}
							}

							pen.x += shaped.advance_.x * scaleFactor + text.letterSpacing_;
							pen.y += shaped.advance_.y * scaleFactor;
						}
					}

					lineTop += lineAdvance;
				}

				/// [EN] Sync this actor's Bounds (every actor has one) to the measured canvas text box: half-extents = box size / 2, centre = box midpoint offset from the text origin. The picker applies the actor's world scale/rotation, so CanvasViewPanel gets a correct box for the drawn text.
				/// [JP] このアクターの Bounds(全アクターが持つ)を計測したキャンバステキストボックスへ同期する: 半径 = ボックスサイズ / 2、中心 = テキスト原点からのボックス中点オフセット。ピッカーがアクターのワールドスケール/回転を掛けるので、CanvasViewPanel は描画済みテキストの正しいボックスを得る。
				if (isSprite && boundsComponentID && textBoxMinX <= textBoxMaxX)
				{
					void* boundsRaw = world.GetComponent(entityID, boundsComponentID);
					if (boundsRaw)
					{
						Bounds* bounds = static_cast<Bounds*>(boundsRaw);
						bounds->center_ = Vector3((textBoxMinX + textBoxMaxX) * 0.5f, (textBoxMinY + textBoxMaxY) * 0.5f, 0.0f);
						bounds->extent_ = Vector3((textBoxMaxX - textBoxMinX) * 0.5f, (textBoxMaxY - textBoxMinY) * 0.5f, 1.0f);
					}
				}
			});
	}

	void FontRenderer::Upload()
	{
		indicesSystem_->SetFontSpriteIndex(spriteBuffer_->Index());
		indicesSystem_->SetFontBillboardIndex(billboardBuffer_->Index());

		if (!spriteInstances_.empty())
		{
			spriteBuffer_->Update(spriteInstances_.data(), static_cast<Uint>(spriteInstances_.size()));
		}

		if (!billboardInstances_.empty())
		{
			billboardBuffer_->Update(billboardInstances_.data(), static_cast<Uint>(billboardInstances_.size()));
		}
	}

	void FontRenderer::DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (spriteInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(fontShader_.GetRootSignature());

		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(fontShader_.GetPipelineStateSprite());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(spriteInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void FontRenderer::DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (billboardInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(fontShader_.GetRootSignature());

		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(fontShader_.GetPipelineStateBillboard());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(billboardInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void FontRenderer::DrawSelectionMaskSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (spriteInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(fontShader_.GetRootSignature());
		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(fontShader_.GetPipelineStateSelectionMaskSprite());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(spriteInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void FontRenderer::DrawSelectionMaskBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (billboardInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(fontShader_.GetRootSignature());
		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(fontShader_.GetPipelineStateSelectionMaskBillboard());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(billboardInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}
}