#include <GraphicsEngine/Renderer/MovieRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/Movie/Movie.h>
#include <GraphicsEngine/Movie/MovieResource.h>
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
	MovieRenderer::MovieRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : movieShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void MovieRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem)
	{
		bindlessHeap_ = bindlessHeap;
		maxCount_ = 1024;

		movieShader_.Create(shaderCache, device);

		indicesSystem_ = &indicesSystem;

		spriteBuffer_ = MakePtr<ReadOnlyStructuredBuffer<MovieSpriteInstance>>(device, bindlessHeap, maxCount_);
		billboardBuffer_ = MakePtr<ReadOnlyStructuredBuffer<MovieBillboardInstance>>(device, bindlessHeap, maxCount_);
		fullscreenBuffer_ = MakePtr<ReadOnlyStructuredBuffer<MovieFullscreenInstance>>(device, bindlessHeap, 32);

		indicesSystem.SetMovieSpriteIndex(spriteBuffer_->Index());
		indicesSystem.SetMovieBillboardIndex(billboardBuffer_->Index());
		indicesSystem.SetMovieFullscreenIndex(fullscreenBuffer_->Index());
	}

	void MovieRenderer::Gather(MovieResource& movieResource, World& world, Vector2 nativeScreenSize, Entity selectedEntity)
	{
		spriteInstances_.clear();
		billboardInstances_.clear();
		fullscreenInstances_.clear();
		hasSelectedSpriteInstance_ = false;
		hasSelectedBillboardInstance_ = false;

		Float spriteReferenceScale = (ScResolution::SC_HD.Height > 0.0f) ? (nativeScreenSize.y / ScResolution::SC_HD.Height) : 1.0f;

		/// [EN] Cached once so each Sprite-mode Movie actor's Bounds can be synced to its canvas rect below (mirrors ImageRenderer's Image -> Bounds sync), giving canvas movies a pickable box in CanvasViewPanel.
		/// [JP] 各スプライトモード Movie アクターの Bounds を下でキャンバス矩形へ同期できるよう一度だけキャッシュする(ImageRenderer の Image -> Bounds 同期と同じ)。これでキャンバス動画が CanvasViewPanel でピック可能なボックスを持つ。
		ComponentID boundsComponentID = ComponentRegistry::GetComponentID<Bounds>();

		Query<Read<Active>, Read<Movie>> query(world);
		query.ForEach([&](EntityID entityID, const Active& active, const Movie& movie)
			{
				if (!active.active_)
				{
					return;
				}

				if (movie.movieID_ == 0 || !movieResource.Contains(movie.movieID_) || !movieResource.HasTexture(movie.movieID_))
				{
					return;
				}

				Uint textureIndex = movieResource.GetTextureIndex(movie.movieID_);
				Int nativeWidth = movieResource.GetWidth(movie.movieID_);
				Int nativeHeight = movieResource.GetHeight(movie.movieID_);

				if (movie.displayMode_ == Movie::DisplayMode::Fullscreen)
				{
					if (nativeWidth <= 0 || nativeHeight <= 0)
					{
						return;
					}

					MovieFullscreenInstance instance{};
					instance.color_ = movie.color_;
					instance.textureIndex_ = textureIndex;
					instance.textureAspect_ = static_cast<Float>(nativeWidth) / static_cast<Float>(nativeHeight);
					fullscreenInstances_.push_back(instance);
					return;
				}

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

				Vector2 size = movie.size_;
				if (size.x <= 0.0f && size.y <= 0.0f)
				{
					size = Vector2(static_cast<Float>(nativeWidth), static_cast<Float>(nativeHeight));
				}

				Uint selected = (selectedEntity.Exists() && actor->GetEntity() == selectedEntity) ? 1 : 0;

				if (movie.displayMode_ == Movie::DisplayMode::Sprite)
				{
					hasSelectedSpriteInstance_ = hasSelectedSpriteInstance_ || selected != 0;

					MovieSpriteInstance instance{};
					instance.position_ = Vector2(worldTranslation.x, worldTranslation.y) * spriteReferenceScale;
					instance.rotation_ = worldRotation.ToEuler().x;
					instance.scale_ = Vector2(worldScale.x, worldScale.y);
					instance.size_ = size;
					instance.pivot_ = movie.pivot_;
					instance.color_ = movie.color_;
					instance.textureIndex_ = textureIndex;
					instance.selected_ = selected;
					spriteInstances_.push_back(instance);

					hasSelectedBillboardInstance_ = hasSelectedBillboardInstance_ || selected != 0;

					MovieBillboardInstance canvasInstance{};
					canvasInstance.position_ = Vector3(100000.0f + worldTranslation.x, 100000.0f + (ScResolution::SC_HD.Height - worldTranslation.y), 100000.0f);
					canvasInstance.rotation_ = Vector3::Zero;
					canvasInstance.scale_ = Vector2(worldScale.x * size.x, worldScale.y * size.y);
					canvasInstance.size_ = size;
					canvasInstance.pivot_ = movie.pivot_;
					canvasInstance.color_ = movie.color_;
					canvasInstance.textureIndex_ = textureIndex;
					canvasInstance.faceCamera_ = 1;
					canvasInstance.selected_ = selected;
					billboardInstances_.push_back(canvasInstance);

					/// [EN] Sync this actor's Bounds (every actor has one) to the canvas movie's local-space rect: half-extents = size/2, centre shifted by the pivot the same way MovieBillboardMS.hlsl shifts the quad (0.5 - pivotNorm on X, pivotNorm - 0.5 on Y). Unscaled - the picker applies the actor's world scale.
					/// [JP] このアクターの Bounds(全アクターが持つ)をキャンバス動画のローカル空間矩形へ同期する: 半径 = size/2、中心は MovieBillboardMS.hlsl がクアッドをずらすのと同じ形(X は 0.5 - pivotNorm、Y は pivotNorm - 0.5)で pivot ぶんずらす。未スケール - ピッカーがアクターのワールドスケールを掛ける。
					if (boundsComponentID)
					{
						void* boundsRaw = world.GetComponent(entityID, boundsComponentID);
						if (boundsRaw)
						{
							Vector2 pivotNorm = (size.x > 0.0f && size.y > 0.0f) ? Vector2(movie.pivot_.x / size.x, movie.pivot_.y / size.y) : Vector2(0.5f, 0.5f);
							Bounds* bounds = static_cast<Bounds*>(boundsRaw);
							bounds->center_ = Vector3((0.5f - pivotNorm.x) * size.x, (pivotNorm.y - 0.5f) * size.y, 0.0f);
							bounds->extent_ = Vector3(size.x * 0.5f, size.y * 0.5f, 1.0f);
						}
					}
				}
				else
				{
					hasSelectedBillboardInstance_ = hasSelectedBillboardInstance_ || selected != 0;

					MovieBillboardInstance instance{};
					instance.position_ = worldTranslation;
					instance.rotation_ = worldRotation.ToEuler();
					instance.scale_ = Vector2(worldScale.x, worldScale.y);
					instance.size_ = size;
					instance.pivot_ = movie.pivot_;
					instance.color_ = movie.color_;
					instance.textureIndex_ = textureIndex;
					instance.faceCamera_ = movie.faceCamera_ ? 1 : 0;
					instance.selected_ = selected;
					billboardInstances_.push_back(instance);
				}
			});
	}

	void MovieRenderer::Upload()
	{
		indicesSystem_->SetMovieSpriteIndex(spriteBuffer_->Index());
		indicesSystem_->SetMovieBillboardIndex(billboardBuffer_->Index());
		indicesSystem_->SetMovieFullscreenIndex(fullscreenBuffer_->Index());

		if (!spriteInstances_.empty())
		{
			spriteBuffer_->Update(spriteInstances_.data(), static_cast<Uint>(spriteInstances_.size()));
		}

		if (!billboardInstances_.empty())
		{
			billboardBuffer_->Update(billboardInstances_.data(), static_cast<Uint>(billboardInstances_.size()));
		}

		if (!fullscreenInstances_.empty())
		{
			fullscreenBuffer_->Update(fullscreenInstances_.data(), static_cast<Uint>(fullscreenInstances_.size()));
		}
	}

	void MovieRenderer::DrawFullscreen(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (fullscreenInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(movieShader_.GetRootSignature());

		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(movieShader_.GetPipelineStateFullscreen());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(fullscreenInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void MovieRenderer::DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (spriteInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(movieShader_.GetRootSignature());

		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(movieShader_.GetPipelineStateSprite());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(spriteInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void MovieRenderer::DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (billboardInstances_.empty())
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(movieShader_.GetRootSignature());

		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(movieShader_.GetPipelineStateBillboard());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(billboardInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void MovieRenderer::DrawSelectionMaskSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (!hasSelectedSpriteInstance_)
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(movieShader_.GetRootSignature());
		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(movieShader_.GetPipelineStateSelectionMaskSprite());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(spriteInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void MovieRenderer::DrawSelectionMaskBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (!hasSelectedBillboardInstance_)
		{
			return;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		cmdList->SetGraphicsRootSignature(movieShader_.GetRootSignature());
		cmdList->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmdList->SetGraphicsRootConstantBufferView(3, structuredIndex);

		cmdList->SetPipelineState(movieShader_.GetPipelineStateSelectionMaskBillboard());
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		Uint groupCount = (static_cast<Uint>(billboardInstances_.size()) + 31) / 32;
		cmdList->DispatchMesh(groupCount, 1, 1);
		ProfilerStats::AddDrawCall();
	}
}
