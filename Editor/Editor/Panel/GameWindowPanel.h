#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/System/CameraSystem.h>

namespace SeedCore
{
	class GameWindowPanel
	{
	public:
		GameWindowPanel(CameraSystem& cameraSystem);
		~GameWindowPanel() = default;

		void Draw(D3D12_GPU_DESCRIPTOR_HANDLE frameBufferHandle, Float toolbarHeight);

		Bool IsFullscreen()const { return fullscreen_; }

		Bool IsImageHovered()const { return imageHovered_; }

	private:
		CameraSystem& cameraSystem_;
		Bool fullscreen_ = false;
		Bool imageHovered_ = false;
	};
}
