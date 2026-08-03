#include <GraphicsEngine/Renderer/EffekseerRenderer.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerManager.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>

namespace SeedCore
{
	namespace
	{
		Effekseer::Matrix44 ToEffekseerMatrix(const Matrix& matrix)
		{
			Effekseer::Matrix44 result;
			for (Int row = 0; row < 4; row++)
			{
				for (Int column = 0; column < 4; column++)
				{
					result.Values[row][column] = matrix.m[row][column];
				}
			}
			return result;
		}
	}

	void EffekseerRenderer::Create(ID3D12Device* device, ID3D12CommandQueue* commandQueue, Uint32 swapBufferCount, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat, Uint32 squareMaxCount)
	{
		graphicsDevice_ = ::EffekseerRendererDX12::CreateGraphicsDevice(device, commandQueue, static_cast<Int32>(swapBufferCount));

		DXGI_FORMAT renderTargetFormats[1] = { colorFormat };
		renderer_ = ::EffekseerRendererDX12::Create(graphicsDevice_, renderTargetFormats, 1, depthFormat, false, static_cast<Int32>(squareMaxCount));

		memoryPool_ = ::EffekseerRenderer::CreateSingleFrameMemoryPool(renderer_->GetGraphicsDevice());
		commandList_ = ::EffekseerRenderer::CreateCommandList(renderer_->GetGraphicsDevice(), memoryPool_);
	}

	void EffekseerRenderer::SetCamera(const SceneConstantBuffer& scene)
	{
		projectionMatrix_ = ToEffekseerMatrix(scene.nonJitterProjection_);
		cameraMatrix_ = ToEffekseerMatrix(scene.view_);
	}

	void EffekseerRenderer::Draw(D3D12CommandList* cmdList, EffekseerManager& manager)
	{
		memoryPool_->NewFrame();

		::EffekseerRendererDX12::BeginCommandList(commandList_, cmdList->Get());
		renderer_->SetCommandList(commandList_);

		renderer_->SetTime(manager.GetTotalTime());
		renderer_->SetProjectionMatrix(projectionMatrix_);
		renderer_->SetCameraMatrix(cameraMatrix_);

		renderer_->BeginRendering();

		Effekseer::Manager::DrawParameter drawParameter;
		drawParameter.ZNear = 0.0f;
		drawParameter.ZFar = 1.0f;
		drawParameter.ViewProjectionMatrix = renderer_->GetCameraProjectionMatrix();
		manager.GetManager()->Draw(drawParameter);

		renderer_->EndRendering();

		renderer_->SetCommandList(nullptr);
		::EffekseerRendererDX12::EndCommandList(commandList_);
	}

	::EffekseerRenderer::RendererRef EffekseerRenderer::GetRenderer()const
	{
		return renderer_;
	}
}
