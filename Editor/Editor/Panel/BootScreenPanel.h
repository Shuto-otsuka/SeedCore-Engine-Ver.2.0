#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/BootConfig.h>
#include <GraphicsEngine/Renderer/BootScreenRenderer.h>

namespace SeedCore
{
	struct EditorContext;

	class BootScreenPanel
	{
	public:
		BootScreenPanel(EditorContext& context);
		~BootScreenPanel() = default;

		void Draw();

		void DrawDetails();

		void Open();

		[[nodiscard]] Bool Focused()const;

	private:
		void DrawCanvas();

	private:
		enum class GizmoDrag
		{
			None,
			Move,
			TopLeft,
			TopRight,
			BottomLeft,
			BottomRight,
		};

	private:
		EditorContext& context_;

		Bool show_ = false;
		Bool isFocused_ = false;

		BootConfig config_;

		ResourcePtr<BootScreenRenderer> renderer_;

		Uint32 screenWidth_ = 1920;
		Uint32 screenHeight_ = 1080;

		Bool imagesDirty_ = true;
		Bool saveRequested_ = false;

		Float progress_ = 0.6f;
		Bool autoPlay_ = true;

		Float zoom_ = 1.0f;
		Vector2 pan_ = Vector2(0.0f, 0.0f);

		GizmoDrag drag_ = GizmoDrag::None;
		Vector2 dragStartMouse_ = Vector2(0.0f, 0.0f);
		Vector2 dragStartOffset_ = Vector2(0.0f, 0.0f);
		Vector4 dragStartRect_ = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	};
}
