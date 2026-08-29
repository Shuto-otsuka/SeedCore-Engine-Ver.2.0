#include <GraphicsEngine/Model/Material/Material.h>
#include <GraphicsEngine/Model/Material/MaterialState.h>

namespace SeedCore
{
	void Material::OnInspectorGUI()
	{
		ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing()));

		if (ImGui::Button("マテリアルビューアを開く"))
		{
			MaterialPanelRequest::openRequested_ = true;
		}
	}
}
