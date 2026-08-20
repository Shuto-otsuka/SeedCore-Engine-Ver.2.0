#include <Editor/Editor/Panel/BoneControllerPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>

namespace SeedCore
{
	BoneControllerPanel::BoneControllerPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void BoneControllerPanel::Open()
	{
		show_ = true;
		ImGui::SetWindowFocus("ボーンコントローラー");
	}

	void BoneControllerPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		ImGui::DockBuilderDockWindow("ボーンコントローラー", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("ボーンコントローラー", &show_))
		{
			ImGui::TextDisabled("未実装");
		}
		ImGui::End();
	}
}
