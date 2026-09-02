#include <Editor/Editor/Panel/SkeletonControllerPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>

namespace SeedCore
{
	SkeletonControllerPanel::SkeletonControllerPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void SkeletonControllerPanel::Open()
	{
		show_ = true;
		ImGui::SetWindowFocus("スケルトンコントローラー");
	}

	void SkeletonControllerPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		ImGui::DockBuilderDockWindow("スケルトンコントローラー", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("スケルトンコントローラー", &show_))
		{
			ImGui::TextDisabled("未実装");
		}
		ImGui::End();
	}
}
