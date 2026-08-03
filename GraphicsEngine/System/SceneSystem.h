#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>

namespace SeedCore
{
	struct SceneConstantBuffer
	{
		Matrix view_;
		Matrix inverseView_;

		Matrix projection_;
		Matrix inverseProjection_;
		Matrix nonJitterProjection_;

		Matrix currentViewProjection_;
		Matrix previousViewProjection_;
		Matrix inverseViewProjection_;
		Matrix nonJitterViewProjection_;

		Matrix previousNonJitterViewProjection_;

		Vector4 cameraPosition_;
		Vector4 cameraFocus_;

		Float fieldOfView_;
		Float nearPlane_;
		Float farPlane_;
		Float sceneConstantPadding1_;

		Float totalTime_;
		Float deltaTime_;
		Vector2 sceneConstantPadding2_;

		Vector2 screenSize_;
		Vector2 inverseScreenSize_;
	};

	class BindlessHeap;

	class SEEDCORE_API SceneSystem
	{
	public:
		SceneSystem(ID3D12Device* device, BindlessHeap* bindlessHeap);
		~SceneSystem();

		void Upload(SceneConstantBuffer buffer);

		Uint GetIndex()const;

	private:
		ResourcePtr<ConstantBuffer<SceneConstantBuffer>> sceneConstantBuffer_;

	};
}