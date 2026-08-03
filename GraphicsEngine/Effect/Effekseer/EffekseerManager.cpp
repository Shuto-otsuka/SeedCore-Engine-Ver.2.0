#include <GraphicsEngine/Effect/Effekseer/EffekseerManager.h>
#include <GraphicsEngine/Renderer/EffekseerRenderer.h>

namespace SeedCore
{
	Bool EffekseerManager::Initialize(EffekseerRenderer& renderer, Uint32 instanceMax)
	{
		::EffekseerRenderer::RendererRef backend = renderer.GetRenderer();
		if (backend == nullptr)
		{
			return false;
		}

		manager_ = Effekseer::Manager::Create(static_cast<Int32>(instanceMax));
		manager_->SetCoordinateSystem(Effekseer::CoordinateSystem::RH);

		fileInterface_ = Effekseer::MakeRefPtr<EffekseerFileInterface>();

		manager_->SetSpriteRenderer(backend->CreateSpriteRenderer());
		manager_->SetRibbonRenderer(backend->CreateRibbonRenderer());
		manager_->SetRingRenderer(backend->CreateRingRenderer());
		manager_->SetTrackRenderer(backend->CreateTrackRenderer());
		manager_->SetModelRenderer(backend->CreateModelRenderer());

		manager_->SetTextureLoader(backend->CreateTextureLoader(fileInterface_));
		manager_->SetModelLoader(backend->CreateModelLoader(fileInterface_));
		manager_->SetMaterialLoader(backend->CreateMaterialLoader(fileInterface_));
		manager_->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>(fileInterface_));

		return true;
	}

	void EffekseerManager::Update(Float deltaTime)
	{
		totalTime_ += deltaTime;

		Effekseer::Manager::UpdateParameter updateParameter;
		updateParameter.DeltaFrame = deltaTime * 60.0f;
		manager_->Update(updateParameter);
	}

	Effekseer::ManagerRef EffekseerManager::GetManager()const
	{
		return manager_;
	}

	Float EffekseerManager::GetTotalTime()const
	{
		return totalTime_;
	}

	EffekseerFileInterface& EffekseerManager::GetFileInterface()
	{
		return *fileInterface_.Get();
	}
}
