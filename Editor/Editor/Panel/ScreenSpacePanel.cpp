#include <Editor/Editor/Panel/ScreenSpacePanel.h>
#include <Editor/Editor/EditorContext.h>

namespace SeedCore
{
	ScreenSpacePanel::ScreenSpacePanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void ScreenSpacePanel::DrawMenuItems()
	{
		if (ImGui::MenuItem("AO：GTAO..."))
		{
			showGroundTruthAmbientOcclusionSettings_ = true;
		}

		if (ImGui::MenuItem("AO：SSAO..."))
		{
			showAmbientOcclusionSettings_ = true;
		}

		if (ImGui::MenuItem("GI：SSGI..."))
		{
			showGlobalIlluminationSettings_ = true;
		}

		if (ImGui::MenuItem("反射：SSR..."))
		{
			showReflectionSettings_ = true;
		}
	}

	void ScreenSpacePanel::DrawWindows()
	{
		DrawGroundTruthAmbientOcclusionSettingsWindow();
		DrawAmbientOcclusionSettingsWindow();
		DrawGlobalIlluminationSettingsWindow();
		DrawReflectionSettingsWindow();
	}

	Bool ScreenSpacePanel::DrawEnableCheckbox(GraphicsEffect effect, Bool siblingClaimed, Bool& enabled)
	{
		ViewportContext& viewport = context_.viewportContext_;

		Bool interactive = !siblingClaimed && GraphicsQuality::IsEnableCheckboxInteractive(effect, GraphicsEffectFamily::ScreenSpace, viewport.qualityPreset_, viewport.raytracing_, viewport.screenSpace_, viewport.rasterization_);

		ImGui::BeginDisabled(!interactive);
		ImGui::Checkbox("有効", &enabled);
		ImGui::EndDisabled();

		if (!interactive && !enabled)
		{
			if (viewport.qualityPreset_ != GraphicsQualityPreset::Custom)
			{
				ImGui::TextDisabled("※品質プリセットが「カスタム」のときのみ変更できます");
			}
			else if (siblingClaimed)
			{
				ImGui::TextDisabled("※同じ効果の別手法が有効です");
			}
			else
			{
				ImGui::TextDisabled("※この効果は他のパイプラインで有効です");
			}
		}

		return enabled;
	}

	Bool ScreenSpacePanel::BeginSettingsWindow(const Char* title, Bool& show)
	{
		if (!show)
		{
			return false;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (!ImGui::Begin(title, &show, flags))
		{
			ImGui::End();
			return false;
		}

		return true;
	}

	void ScreenSpacePanel::EndSettingsWindow(Bool& show)
	{
		ImGui::Spacing();

		Float buttonWidth = 120.0f;
		ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
		if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
		{
			show = false;
		}

		ImGui::End();
	}

	void ScreenSpacePanel::DrawGroundTruthAmbientOcclusionSettingsWindow()
	{
		if (!BeginSettingsWindow("スクリーンスペース：AO(GTAO)", showGroundTruthAmbientOcclusionSettings_))
		{
			return;
		}

		GroundTruthAmbientOcclusionSettings& settings = context_.viewportContext_.screenSpace_.groundTruthAmbientOcclusion_;
		Bool enabled = DrawEnableCheckbox(GraphicsEffect::AmbientOcclusion, context_.viewportContext_.screenSpace_.ambientOcclusionEnabled_, context_.viewportContext_.screenSpace_.groundTruthAmbientOcclusionEnabled_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!enabled);
		ImGui::PushID("GTAO");
		ImGui::SliderFloat("半径", &settings.radius_, 0.05f, 5.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("減衰幅", &settings.falloffRange_, 0.0f, 1.0f, "%.3f");
		ImGui::SliderFloat("強調(指数)", &settings.power_, 0.25f, 4.0f, "%.2f");

		Int sliceCount = static_cast<Int>(settings.sliceCount_);
		if (ImGui::SliderInt("スライス数", &sliceCount, 1, 8))
		{
			settings.sliceCount_ = static_cast<Uint32>(sliceCount);
		}

		Int stepsPerSlice = static_cast<Int>(settings.stepsPerSlice_);
		if (ImGui::SliderInt("スライスあたりステップ数", &stepsPerSlice, 1, 8))
		{
			settings.stepsPerSlice_ = static_cast<Uint32>(stepsPerSlice);
		}
		ImGui::PopID();
		ImGui::EndDisabled();

		EndSettingsWindow(showGroundTruthAmbientOcclusionSettings_);
	}

	void ScreenSpacePanel::DrawAmbientOcclusionSettingsWindow()
	{
		if (!BeginSettingsWindow("スクリーンスペース：AO(SSAO)", showAmbientOcclusionSettings_))
		{
			return;
		}

		ScreenSpaceAmbientOcclusionSettings& settings = context_.viewportContext_.screenSpace_.ambientOcclusion_;
		Bool enabled = DrawEnableCheckbox(GraphicsEffect::AmbientOcclusion, context_.viewportContext_.screenSpace_.groundTruthAmbientOcclusionEnabled_, context_.viewportContext_.screenSpace_.ambientOcclusionEnabled_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!enabled);
		ImGui::PushID("SSAO");
		ImGui::SliderFloat("半径", &settings.radius_, 0.05f, 5.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("バイアス", &settings.bias_, 0.0f, 0.2f, "%.4f");
		ImGui::SliderFloat("強さ", &settings.intensity_, 0.0f, 4.0f, "%.2f");
		ImGui::SliderFloat("強調(指数)", &settings.power_, 0.25f, 4.0f, "%.2f");

		Int sampleCount = static_cast<Int>(settings.sampleCount_);
		if (ImGui::SliderInt("サンプル数", &sampleCount, 4, 64))
		{
			settings.sampleCount_ = static_cast<Uint32>(sampleCount);
		}
		ImGui::PopID();
		ImGui::EndDisabled();

		EndSettingsWindow(showAmbientOcclusionSettings_);
	}

	void ScreenSpacePanel::DrawGlobalIlluminationSettingsWindow()
	{
		if (!BeginSettingsWindow("スクリーンスペース：GI(SSGI)", showGlobalIlluminationSettings_))
		{
			return;
		}

		ScreenSpaceGlobalIlluminationSettings& settings = context_.viewportContext_.screenSpace_.globalIllumination_;
		Bool enabled = DrawEnableCheckbox(GraphicsEffect::GlobalIllumination, false, context_.viewportContext_.screenSpace_.globalIlluminationEnabled_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!enabled);
		ImGui::PushID("SSGI");
		ImGui::SliderFloat("強度", &settings.intensity_, 0.0f, 4.0f, "%.2f");
		ImGui::SliderFloat("レイ長", &settings.rayLength_, 0.1f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("厚み", &settings.thickness_, 0.01f, 2.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

		Int sampleCount = static_cast<Int>(settings.sampleCount_);
		if (ImGui::SliderInt("サンプル数", &sampleCount, 2, 32))
		{
			settings.sampleCount_ = static_cast<Uint32>(sampleCount);
		}
		ImGui::PopID();
		ImGui::EndDisabled();

		EndSettingsWindow(showGlobalIlluminationSettings_);
	}

	void ScreenSpacePanel::DrawReflectionSettingsWindow()
	{
		if (!BeginSettingsWindow("スクリーンスペース：反射(SSR)", showReflectionSettings_))
		{
			return;
		}

		ScreenSpaceReflectionSettings& settings = context_.viewportContext_.screenSpace_.reflection_;
		Bool enabled = DrawEnableCheckbox(GraphicsEffect::Reflection, false, context_.viewportContext_.screenSpace_.reflectionEnabled_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!enabled);
		ImGui::PushID("SSR");
		ImGui::SliderFloat("強さ", &settings.strength_, 0.0f, 1.0f);
		ImGui::SliderFloat("最大ラフネス", &settings.maxRoughness_, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("厚み", &settings.thickness_, 0.01f, 2.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

		Int maxStepCount = static_cast<Int>(settings.maxStepCount_);
		if (ImGui::SliderInt("最大ステップ数", &maxStepCount, 8, 256))
		{
			settings.maxStepCount_ = static_cast<Uint32>(maxStepCount);
		}
		ImGui::PopID();
		ImGui::EndDisabled();

		EndSettingsWindow(showReflectionSettings_);
	}
}
