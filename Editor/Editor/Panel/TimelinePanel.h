#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;
	class Animator;

	class TimelinePanel
	{
	public:
		TimelinePanel(EditorContext& context);

		void Draw();

		void Open();

		void SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle);

	private:
		EditorContext& context_;

		Bool show_ = false;
		Animator* target_ = nullptr;

		Size selectedAnimationIndex_ = SIZE_MAX;
		Float scrubTime_ = 0.0f;
		Bool isPlaying_ = false;

		Size selectedNotifyIndex_ = SIZE_MAX;
		Size selectedSpeedKeyIndex_ = SIZE_MAX;
		Bool isDraggingKeyframe_ = false;
		Bool speedGraphActive_ = false;

		D3D12_GPU_DESCRIPTOR_HANDLE previewHandle_{};
	};
}
