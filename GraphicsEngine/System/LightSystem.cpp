#include <GraphicsEngine/System/LightSystem.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/Light/DirectionalLight.h>
#include <GraphicsEngine/System/CelestialSystem.h>
#include <GraphicsEngine/System/WeatherSystem.h>
#include <GraphicsEngine/Light/PointLight.h>
#include <GraphicsEngine/Light/SpotLight.h>
#include <GraphicsEngine/Light/RectangleLight.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	LightSystem::LightSystem(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height) : bindlessHeap_(bindlessHeap)
	{
		lightConstantBuffer_ = MakePtr<ConstantBuffer<LightConstantBuffer>>(device, bindlessHeap);

		clusterAssignConstantBuffer_ = MakePtr<ConstantBuffer<ClusterAssignConstantBuffer>>(device, bindlessHeap);

		pointLightBuffer_ = MakePtr<ReadOnlyStructuredBuffer<PointLightData>>(device, bindlessHeap, maxPointLights_);

		spotLightBuffer_ = MakePtr<ReadOnlyStructuredBuffer<SpotLightData>>(device, bindlessHeap, maxSpotLights_);

		rectLightBuffer_ = MakePtr<ReadOnlyStructuredBuffer<RectLightData>>(device, bindlessHeap, maxRectLights_);

		CreateClusterResources(device, bindlessHeap, width, height);

		rootSignatureHandle_ = rootSignature.GetOrCreate(device);
		clusterRootSignature_ = rootSignature.Get(rootSignatureHandle_);

		clusterAssignShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Light/ClusterAssignCS.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = clusterRootSignature_->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(clusterAssignShader_)->Bytecode();
		clusterAssignPipelineStateObjectHandle_ = pipelineStateObject.GetOrCreate(device, psokey);
		clusterAssignPipelineStateObject_ = pipelineStateObject.Get(clusterAssignPipelineStateObjectHandle_);
	}

	void LightSystem::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(clusterDataUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(clusterDataClearUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(clusterDataShaderResourceViewIndex_);
		bindlessHeap->FreeIndex(clusterLightListUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(clusterLightListShaderResourceViewIndex_);

		clusterDataResource_.Reset();
		clusterLightListResource_.Reset();
	}

	void LightSystem::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);
		CreateClusterResources(device, bindlessHeap, width, height);
	}

	void LightSystem::CreateClusterResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;

		clusterCountX_ = (width + clusterTileSize_ - 1) / clusterTileSize_;
		clusterCountY_ = (height + clusterTileSize_ - 1) / clusterTileSize_;
		totalClusters_ = clusterCountX_ * clusterCountY_ * clusterDepthSlices_;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, false);

		HRESULT hr{ S_OK };
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		{
			Uint64 bufferSize = static_cast<Uint64>(totalClusters_) * sizeof(ClusterData);
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = bufferSize;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&clusterDataResource_));
			SC_HR_CHECK(hr, "クラスターデータリソースの生成に失敗しました");

			clusterDataUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.NumElements = totalClusters_;
			unorderedAccessViewDesc.Buffer.StructureByteStride = sizeof(ClusterData);
			device->CreateUnorderedAccessView(clusterDataResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(clusterDataUnorderedAccessViewIndex_));

			/// [EN] ClearUnorderedAccessView* cannot target a structured UAV, so the
			///      clear goes through a RAW (R32_TYPELESS) view of the same buffer.
			///      Both a shader-visible (GPU handle) and a clear-heap (CPU handle)
			///      copy are required, created identically.
			/// [JP] ClearUnorderedAccessView* は構造化UAVを対象にできないため、同じ
			///      バッファの RAW(R32_TYPELESS) ビュー経由でクリアする。GPUハンドル用
			///      (shader-visible)と CPUハンドル用(clearHeap)を同一設定で用意する。
			D3D12_UNORDERED_ACCESS_VIEW_DESC clearUnorderedAccessViewDesc{};
			clearUnorderedAccessViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			clearUnorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			clearUnorderedAccessViewDesc.Buffer.NumElements = totalClusters_ * (sizeof(ClusterData) / sizeof(Uint));
			clearUnorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

			clusterDataClearUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(clusterDataResource_.Get(), nullptr, &clearUnorderedAccessViewDesc, bindlessHeap->CPUHandle(clusterDataClearUnorderedAccessViewIndex_));

			clusterDataClearIndex_ = clearHeap_.AllocateIndex();
			device->CreateUnorderedAccessView(clusterDataResource_.Get(), nullptr, &clearUnorderedAccessViewDesc, clearHeap_.CPUHandle(clusterDataClearIndex_));

			clusterDataShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Buffer.NumElements = totalClusters_;
			shaderResourceViewDesc.Buffer.StructureByteStride = sizeof(ClusterData);
			device->CreateShaderResourceView(clusterDataResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(clusterDataShaderResourceViewIndex_));
		}

		{
			Uint64 totalIndices = static_cast<Uint64>(totalClusters_) * clusterStride_;
			Uint64 bufferSize = totalIndices * sizeof(Uint);
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = bufferSize;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&clusterLightListResource_));
			SC_HR_CHECK(hr, "クラスターライトリストリソースの生成に失敗しました");

			clusterLightListUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.NumElements = static_cast<Uint>(totalIndices);
			unorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
			device->CreateUnorderedAccessView(clusterLightListResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(clusterLightListUnorderedAccessViewIndex_));

			clusterLightListClearIndex_ = clearHeap_.AllocateIndex();
			device->CreateUnorderedAccessView(clusterLightListResource_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clusterLightListClearIndex_));

			clusterLightListShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Buffer.NumElements = static_cast<Uint>(totalIndices);
			shaderResourceViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		device->CreateShaderResourceView(clusterLightListResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(clusterLightListShaderResourceViewIndex_));
		}
	}

	void LightSystem::Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, World& world, const CelestialResult* celestial, const WeatherGpuState* weather)
	{
		pointLights_.clear();
		spotLights_.clear();
		rectLights_.clear();

		lightConstantData_.directionalIntensity_ = 0.0f;
		lightConstantData_.directionalColor_ = Color(0, 0, 0, 0);
		lightConstantData_.directionalDirection_ = Vector3(0.0f, -1.0f, 0.0f);

		lightConstantData_.moonDirection_ = Vector3(0.0f, 1.0f, 0.0f);
		lightConstantData_.moonIntensity_ = 0.0f;
		lightConstantData_.moonColor_ = Color(0, 0, 0, 0);
		lightConstantData_.moonPhase_ = 0.0f;
		lightConstantData_.nightFactor_ = 0.0f;

		if (celestial)
		{
			lightConstantData_.directionalDirection_ = celestial->sunDirection_;
			lightConstantData_.directionalIntensity_ = celestial->sunIntensity_;
			lightConstantData_.directionalColor_ = celestial->sunColor_;

			lightConstantData_.moonDirection_ = celestial->moonDirection_;
			lightConstantData_.moonIntensity_ = celestial->moonIntensity_;
			lightConstantData_.moonColor_ = celestial->moonColor_;
			lightConstantData_.moonPhase_ = celestial->moonPhase_;
			lightConstantData_.nightFactor_ = celestial->nightFactor_;
			lightConstantData_.moonAngularRadius_ = celestial->moonAngularRadius_;
		}

		lightConstantData_.wetness_ = 0.0f;
		lightConstantData_.snowCoverage_ = 0.0f;
		lightConstantData_.thunderFlash_ = 0.0f;
		lightConstantData_.snowIntensity_ = 0.0f;
		lightConstantData_.thunderSeed_ = 0.0f;

		if (weather)
		{
			lightConstantData_.wetness_ = weather->wetness_;
			lightConstantData_.snowCoverage_ = weather->snowCoverage_;
			lightConstantData_.thunderFlash_ = weather->thunderFlash_;
			lightConstantData_.snowIntensity_ = weather->snowIntensity_;
			lightConstantData_.thunderSeed_ = weather->thunderSeed_;
		}

		Bool hasDirectional = celestial != nullptr;

		/// [EN] All four queries below read the parent-composed world
		///      transform from TransformSystem (Actor::GetWorldMatrix()),
		///      same as ModelRenderer/ImageRenderer/FontRenderer,
		///      instead of the actor's own local
		///      Position/direction fields - so parented lights (position,
		///      and for spot/rect/directional, orientation too) follow
		///      their parent. TransformNormal rotates a local direction/up
		///      vector by the world matrix without translation.
		/// [JP] 以下の4クエリはすべて、アクター自身のローカル
		///      Position/方向フィールドではなく、TransformSystem
		///      (Actor::GetWorldMatrix())が計算した親合成済みのワールド変換を
		///      読む（ModelRenderer/ImageRenderer/FontRenderer と同様）。
		///      これで親付けされたライトの位置、
		///      および spot/rect/directional では向きも親に追従する。
		///      TransformNormal はローカルの方向/up ベクトルを平行移動なしで
		///      ワールド行列により回転させる。
		Query<Read<Active>, Read<DirectionalLight>> directionalQuery(world);
		directionalQuery.ForEach([&](EntityID entityID, const Active& active, const DirectionalLight& light)
			{
				if (!active.active_ || hasDirectional)
				{
					return;
				}

				hasDirectional = true;

				Vector3 direction = light.direction_;
				Actor* actor = world.GetActor(entityID);
				if (actor)
				{
					direction = Vector3::TransformNormal(direction, actor->GetWorldMatrix());
				}
				direction.Normalize();

				lightConstantData_.directionalDirection_ = direction;
				lightConstantData_.directionalIntensity_ = light.intensity_;
				lightConstantData_.directionalColor_ = light.color_;
			});

		Query<Read<Active>, Read<PointLight>> pointQuery(world);
		pointQuery.ForEach([&](EntityID entityID, const Active& active, const PointLight& light)
			{
				if (!active.active_)
				{
					return;
				}

				Actor* actor = world.GetActor(entityID);
				if (!actor)
				{
					return;
				}

				PointLightData pointLightData{};
				pointLightData.position_ = actor->GetWorldMatrix().Translation();
				pointLightData.range_ = light.range_;
				pointLightData.color_ = light.color_;
				pointLightData.intensity_ = light.intensity_;
				pointLights_.push_back(pointLightData);
			});

		Query<Read<Active>, Read<SpotLight>> spotQuery(world);
		spotQuery.ForEach([&](EntityID entityID, const Active& active, const SpotLight& light)
			{
				if (!active.active_)
				{
					return;
				}

				Actor* actor = world.GetActor(entityID);
				if (!actor)
				{
					return;
				}

				Matrix worldMatrix = actor->GetWorldMatrix();
				Vector3 direction = Vector3::TransformNormal(light.direction_, worldMatrix);
				direction.Normalize();

				SpotLightData spotLightData{};
				spotLightData.position_ = worldMatrix.Translation();
				spotLightData.range_ = light.range_;
				spotLightData.direction_ = direction;
				spotLightData.cosHalfAngle_ = Cos(ToRadians(light.spotAngle_ * 0.5f));
				spotLightData.color_ = light.color_;
				spotLightData.intensity_ = light.intensity_;
				spotLightData.softness_ = light.softness_;
				spotLights_.push_back(spotLightData);
			});

		Query<Read<Active>, Read<RectangleLight>> rectQuery(world);
		rectQuery.ForEach([&](EntityID entityID, const Active& active, const RectangleLight& light)
			{
				if (!active.active_)
				{
					return;
				}

				Actor* actor = world.GetActor(entityID);
				if (!actor)
				{
					return;
				}

				Matrix worldMatrix = actor->GetWorldMatrix();

				/// [JP] 向きベクトルから正規直交基底を作る。normal=正面、right/up は
				///      矩形の辺方向。up_ が normal_ とほぼ平行だと right が縮退するので
				///      その場合はフォールバックする。
				Vector3 normal = Vector3::TransformNormal(light.direction_, worldMatrix);
				normal.Normalize();

				Vector3 upHint = Vector3::TransformNormal(light.up_, worldMatrix);

				Vector3 right = upHint.Cross(normal);
				if (right.LengthSquared() < 1e-6f)
				{
					right = Vector3::Right;
				}
				right.Normalize();

				Vector3 up = normal.Cross(right);
				up.Normalize();

				RectLightData rectLightData{};
				rectLightData.position_ = worldMatrix.Translation();
				rectLightData.intensity_ = light.intensity_;
				rectLightData.right_ = right;
				rectLightData.halfWidth_ = light.width_ * 0.5f;
				rectLightData.up_ = up;
				rectLightData.halfHeight_ = light.height_ * 0.5f;
				rectLightData.normal_ = normal;
				rectLightData.range_ = light.range_;
				rectLightData.color_ = light.color_;
				rectLights_.push_back(rectLightData);
			});

		/// [EN] Pull KHR_lights_punctual point/spot lights embedded in loaded
		///      glTF models, transformed by the owning Actor's world matrix
		///      (TransformSystem's Actor::GetWorldMatrix(), same source of
		///      truth as ModelRenderer). Directional KHR lights are never
		///      stored in Crister::lights_ (see ModelLoader::FetchLights),
		///      so only point/spot show up here - matching the cluster-only
		///      scope; this engine's directional light stays scene-authored.
		/// [JP] ロード済み glTF モデルに埋め込まれた KHR_lights_punctual の
		///      ポイント/スポットライトを、所有 Actor のワールド行列
		///      （TransformSystem の Actor::GetWorldMatrix()、ModelRenderer と
		///      同じ情報源）で変換して取り込む。ディレクショナルの KHR ライトは
		///      Crister::lights_ に格納されない（ModelLoader::FetchLights 参照）
		///      ため、ここに出てくるのはポイント/スポットのみ＝クラスター限定の
		///      意図と一致する。このエンジンのディレクショナルライトは
		///      引き続きシーン単位で設定する。
		Query<Read<Active>, Read<Mesh>> modelLightQuery(world);
		modelLightQuery.ForEach([&](EntityID entityID, const Active& active, const Mesh& mesh)
			{
				if (!active.active_)
				{
					return;
				}

				Handle<Crister> cristerHandle = modelResource.GetHandle(mesh.meshID_);
				if (cristerHandle.empty())
				{
					return;
				}

				Crister* crister = modelResource.Resolve(loaderSystem, cristerHandle);
				if (!crister || crister->Lights().empty())
				{
					return;
				}

				Actor* actor = world.GetActor(entityID);
				if (!actor)
				{
					return;
				}

				Matrix worldMatrix = actor->GetWorldMatrix();

				for (const PunctualLight& light : crister->Lights())
				{
					Vector3 worldPosition = Vector3::Transform(light.position_, worldMatrix);
					Vector3 worldDirection = Vector3::TransformNormal(light.direction_, worldMatrix);
					worldDirection.Normalize();

					Color color(light.colorRGB_[0], light.colorRGB_[1], light.colorRGB_[2], 1.0f);

					if (light.type_ == PunctualLight::Type::Point)
					{
						PointLightData pointLightData{};
						pointLightData.position_ = worldPosition;
						pointLightData.range_ = light.range_;
						pointLightData.color_ = color;
						pointLightData.intensity_ = light.intensity_;
						pointLights_.push_back(pointLightData);
					}
					else
					{
						SpotLightData spotLightData{};
						spotLightData.position_ = worldPosition;
						spotLightData.range_ = light.range_;
						spotLightData.direction_ = worldDirection;
						spotLightData.cosHalfAngle_ = Cos(light.outerConeAngle_);
						spotLightData.color_ = color;
						spotLightData.intensity_ = light.intensity_;

						/// [EN] Approximates glTF's inner/outer cone falloff
						///      band as this engine's single softness scalar
						///      (0 = hard edge at outerConeAngle).
						/// [JP] glTF の inner/outer コーンによる減衰帯を、この
						///      エンジンの単一 softness スカラーで近似する
						///      (0 = outerConeAngle で急峻に切れる)。
						Float softness = 0.0f;
						if (light.outerConeAngle_ > 0.0f)
						{
							Float ratio = (light.outerConeAngle_ - light.innerConeAngle_) / light.outerConeAngle_;
							softness = (ratio < 0.0f) ? 0.0f : ((ratio > 1.0f) ? 1.0f : ratio);
						}
						spotLightData.softness_ = softness;

						spotLights_.push_back(spotLightData);
					}
				}
			});
	}

	void LightSystem::Upload()
	{
		lightConstantData_.pointLightCount_ = static_cast<Uint>(pointLights_.size());
		lightConstantData_.spotLightCount_ = static_cast<Uint>(spotLights_.size());
		lightConstantData_.pointLightShaderResourceViewIndex_ = pointLightBuffer_->Index();
		lightConstantData_.spotLightShaderResourceViewIndex_ = spotLightBuffer_->Index();
		lightConstantData_.clusterDataShaderResourceViewIndex_ = clusterDataShaderResourceViewIndex_;
		lightConstantData_.clusterLightListShaderResourceViewIndex_ = clusterLightListShaderResourceViewIndex_;
		lightConstantData_.clusterCountX_ = clusterCountX_;
		lightConstantData_.clusterCountY_ = clusterCountY_;
		lightConstantData_.rectLightCount_ = static_cast<Uint>(rectLights_.size());
		lightConstantData_.rectLightShaderResourceViewIndex_ = rectLightBuffer_->Index();

		lightConstantBuffer_->Update(lightConstantData_);

		if (!pointLights_.empty())
		{
			pointLightBuffer_->Update(pointLights_.data(), static_cast<Uint>(pointLights_.size()));
		}

		if (!spotLights_.empty())
		{
			spotLightBuffer_->Update(spotLights_.data(), static_cast<Uint>(spotLights_.size()));
		}

		if (!rectLights_.empty())
		{
			rectLightBuffer_->Update(rectLights_.data(), static_cast<Uint>(rectLights_.size()));
		}
	}

	void LightSystem::DispatchCluster(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredAddress)
	{
		Uint totalLights = static_cast<Uint>(pointLights_.size() + spotLights_.size() + rectLights_.size());
		if (totalLights == 0)
		{
			return;
		}

		auto* cmd = cmdList->Get();

		const Uint clearValues[4] = { 0, 0, 0, 0 };
		cmd->ClearUnorderedAccessViewUint(bindlessHeap_->GPUHandle(clusterDataClearUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clusterDataClearIndex_), clusterDataResource_.Get(), clearValues, 0, nullptr);
		cmd->ClearUnorderedAccessViewUint(bindlessHeap_->GPUHandle(clusterLightListUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clusterLightListClearIndex_), clusterLightListResource_.Get(), clearValues, 0, nullptr);

		clusterAssignConstantData_.clusterDataUnorderedAccessViewIndex_ = clusterDataUnorderedAccessViewIndex_;
		clusterAssignConstantData_.clusterLightListUnorderedAccessViewIndex_ = clusterLightListUnorderedAccessViewIndex_;
		clusterAssignConstantData_.pointLightShaderResourceViewIndex_ = pointLightBuffer_->Index();
		clusterAssignConstantData_.spotLightShaderResourceViewIndex_ = spotLightBuffer_->Index();
		clusterAssignConstantData_.pointLightCount_ = static_cast<Uint>(pointLights_.size());
		clusterAssignConstantData_.spotLightCount_ = static_cast<Uint>(spotLights_.size());
		clusterAssignConstantData_.rectLightShaderResourceViewIndex_ = rectLightBuffer_->Index();
		clusterAssignConstantData_.rectLightCount_ = static_cast<Uint>(rectLights_.size());
		clusterAssignConstantData_.totalClusters_ = totalClusters_;
		clusterAssignConstantData_.clusterCountX_ = clusterCountX_;
		clusterAssignConstantData_.clusterCountY_ = clusterCountY_;
		clusterAssignConstantBuffer_->Update(clusterAssignConstantData_);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootSignature(clusterRootSignature_->Get());
		cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
		cmd->SetComputeRootConstantBufferView(2, constantIndex);
		cmd->SetComputeRootConstantBufferView(3, structuredAddress);

		cmd->SetPipelineState(clusterAssignPipelineStateObject_.Get());

		Uint dispatchX = (totalLights + 63) / 64;
		cmd->Dispatch(dispatchX, 1, 1);

		D3D12_RESOURCE_BARRIER barriers[2]{};
		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barriers[0].UAV.pResource = clusterDataResource_.Get();
		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barriers[1].UAV.pResource = clusterLightListResource_.Get();
		cmd->ResourceBarrier(2, barriers);
	}

	Uint LightSystem::GetIndex()const
	{
		return lightConstantBuffer_->GetIndex();
	}

	Uint LightSystem::GetClusterConstantIndex()const
	{
		return clusterAssignConstantBuffer_->GetIndex();
	}
}
