#include <GraphicsEngine/System/SceneSystem.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	SceneSystem::SceneSystem(ID3D12Device* device, BindlessHeap* bindlessHeap)
	{
		sceneConstantBuffer_ = MakePtr<ConstantBuffer<SceneConstantBuffer>>(device, bindlessHeap);
	}

	SceneSystem::~SceneSystem()
	{
		if(sceneConstantBuffer_)
		{
			sceneConstantBuffer_.reset();
			sceneConstantBuffer_ = nullptr;
		}
	}

	void SceneSystem::Upload(SceneConstantBuffer buffer)
	{
		sceneConstantBuffer_->Update(buffer);
	}

	/// [EN] Frame-ring buffer: the index changes per frame — never cache it.
	/// [JP] フレームリングバッファ: インデックスは毎フレーム変わる — キャッシュ禁止。
	Uint SceneSystem::GetIndex()const
	{
		return sceneConstantBuffer_->GetIndex();
	}
}