#include <Editor/Editor/Panel/MaterialPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>

namespace SeedCore
{
	MaterialPanel::MaterialPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void MaterialPanel::Open()
	{
		show_ = true;
		ImGui::SetWindowFocus("マテリアル");
	}

	void MaterialPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		ImGui::DockBuilderDockWindow("マテリアル", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("マテリアル", &show_))
		{
			ImGui::TextDisabled("未実装");
		}
		ImGui::End();
	}
}
