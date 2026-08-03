#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/System/SceneSystem.h>

namespace SeedCore
{
	class D3D12CommandList;
	class EffekseerManager;

	class EffekseerRenderer :public NonCopyable
	{
	public:
		EffekseerRenderer() = default;
		~EffekseerRenderer() = default;

		void Create(ID3D12Device* device, ID3D12CommandQueue* commandQueue, Uint32 swapBufferCount, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat, Uint32 squareMaxCount = 8000);

		void SetCamera(const SceneConstantBuffer& scene);

		void Draw(D3D12CommandList* cmdList, EffekseerManager& manager);

		[[nodiscard]] ::EffekseerRenderer::RendererRef GetRenderer()const;

	private:
		Effekseer::Backend::GraphicsDeviceRef graphicsDevice_;

		::EffekseerRenderer::RendererRef renderer_;

		Effekseer::RefPtr<::EffekseerRenderer::SingleFrameMemoryPool> memoryPool_;

		Effekseer::RefPtr<::EffekseerRenderer::CommandList> commandList_;

		Effekseer::Matrix44 projectionMatrix_;

		Effekseer::Matrix44 cameraMatrix_;
	};
}
