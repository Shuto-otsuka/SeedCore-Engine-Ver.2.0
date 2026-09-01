#include <Editor/Editor/Panel/RasterizationPanel.h>
#include <Editor/Editor/EditorContext.h>

namespace SeedCore
{
	RasterizationPanel::RasterizationPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void RasterizationPanel::DrawMenuItems()
	{
		if (ImGui::MenuItem("影：VSM(Virtual Shadow Map)..."))
		{
			showVirtualShadowMapSettings_ = true;
		}

		if (ImGui::MenuItem("影：CSM(Cascaded Shadow Map)..."))
		{
			showCascadedShadowMapSettings_ = true;
		}

		if (ImGui::MenuItem("反射：SDFR(SDFレイマーチ)..."))
		{
			showSignedDistanceFieldReflectionSettings_ = true;
		}

		if (ImGui::MenuItem("GI：DDGI..."))
		{
			showDynamicDiffuseGlobalIlluminationSettings_ = true;
		}
	}

	void RasterizationPanel::DrawWindows()
	{
		DrawVirtualShadowMapSettingsWindow();
		DrawCascadedShadowMapSettingsWindow();
		DrawSignedDistanceFieldReflectionSettingsWindow();
		DrawDynamicDiffuseGlobalIlluminationSettingsWindow();
	}

	Bool RasterizationPanel::DrawEnableCheckbox(GraphicsEffect effect, Bool siblingClaimed, Bool& enabled)
	{
		ViewportContext& viewport = context_.viewportContext_;

		Bool interactive = !siblingClaimed && GraphicsQuality::IsEnableCheckboxInteractive(effect, GraphicsEffectFamily::Rasterization, viewport.qualityPreset_, viewport.raytracing_, viewport.screenSpace_, viewport.rasterization_);

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

	Bool RasterizationPanel::BeginSettingsWindow(const Char* title, Bool& show)
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

	void RasterizationPanel::EndSettingsWindow(Bool& show)
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

	void RasterizationPanel::DrawVirtualShadowMapSettingsWindow()
	{
		if (!BeginSettingsWindow("ラスタライゼーション：影(VSM)", showVirtualShadowMapSettings_))
		{
			return;
		}

		VirtualShadowMapSettings& settings = context_.viewportContext_.rasterization_.virtualShadowMap_;
		Bool enabled = DrawEnableCheckbox(GraphicsEffect::Shadow, context_.viewportContext_.rasterization_.cascadedShadowMapEnabled_, context_.viewportContext_.rasterization_.virtualShadowMapEnabled_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!enabled);
		ImGui::PushID("VSM");

		Int virtualResolutionLog2 = static_cast<Int>(settings.virtualResolutionLog2_);
		if (ImGui::SliderInt("仮想解像度(2^N)", &virtualResolutionLog2, 11, 16))
		{
			settings.virtualResolutionLog2_ = static_cast<Uint32>(virtualResolutionLog2);
		}

		Int pagePoolSizeMib = static_cast<Int>(settings.pagePoolSizeMib_);
		if (ImGui::SliderInt("ページプール(MiB)", &pagePoolSizeMib, 16, 512))
		{
			settings.pagePoolSizeMib_ = static_cast<Uint32>(pagePoolSizeMib);
		}

		Int clipmapLevelCount = static_cast<Int>(settings.clipmapLevelCount_);
		if (ImGui::SliderInt("クリップマップ段数", &clipmapLevelCount, 1, 12))
		{
			settings.clipmapLevelCount_ = static_cast<Uint32>(clipmapLevelCount);
		}

		ImGui::SliderFloat("ソフトシャドウ半径", &settings.softnessRadius_, 0.0f, 4.0f, "%.2f");
		ImGui::SliderFloat("深度バイアス", &settings.depthBias_, 0.0001f, 0.05f, "%.4f", ImGuiSliderFlags_Logarithmic);
		ImGui::PopID();
		ImGui::EndDisabled();

		EndSettingsWindow(showVirtualShadowMapSettings_);
	}

	void RasterizationPanel::DrawCascadedShadowMapSettingsWindow()
	{
		if (!BeginSettingsWindow("ラスタライゼーション：影(CSM)", showCascadedShadowMapSettings_))
		{
			return;
		}

		CascadedShadowMapSettings& settings = context_.viewportContext_.rasterization_.cascadedShadowMap_;
		Bool enabled = DrawEnableCheckbox(GraphicsEffect::Shadow, context_.viewportContext_.rasterization_.virtualShadowMapEnabled_, context_.viewportContext_.rasterization_.cascadedShadowMapEnabled_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!enabled);
		ImGui::PushID("CSM");

		Int cascadeCount = static_cast<Int>(settings.cascadeCount_);
		if (ImGui::SliderInt("カスケード数", &cascadeCount, 1, 4))
		{
			settings.cascadeCount_ = static_cast<Uint32>(cascadeCount);
		}

		ImGui::SliderFloat("分割λ(対数/一様のブレンド)", &settings.splitLambda_, 0.0f, 1.0f, "%.2f");

		Int pcfKernelSize = static_cast<Int>(settings.pcfKernelSize_);
		if (ImGui::SliderInt("PCFカーネル", &pcfKernelSize, 1, 9))
		{
			settings.pcfKernelSize_ = static_cast<Uint32>(pcfKernelSize);
		}

		ImGui::SliderFloat("カスケードブレンド幅", &settings.cascadeBlendWidth_, 0.0f, 0.5f, "%.3f");
		ImGui::SliderFloat("深度バイアス", &settings.depthBias_, 0.0001f, 0.05f, "%.4f", ImGuiSliderFlags_Logarithmic);
		ImGui::Checkbox("カスケード安定化", &settings.stabilize_);
		ImGui::PopID();
		ImGui::EndDisabled();

		EndSettingsWindow(showCascadedShadowMapSettings_);
	}

	void RasterizationPanel::DrawSignedDistanceFieldReflectionSettingsWindow()
	{
		if (!BeginSettingsWindow("ラスタライゼーション：反射(SDFR)", showSignedDistanceFieldReflectionSettings_))
		{
			return;
		}

		SignedDistanceFieldReflectionSettings& settings = context_.viewportContext_.rasterization_.signedDistanceFieldReflection_;
		Bool enabled = DrawEnableCheckbox(GraphicsEffect::Reflection, false, context_.viewportContext_.rasterization_.signedDistanceFieldReflectionEnabled_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!enabled);
		ImGui::PushID("SDFR");
		ImGui::SliderFloat("強さ", &settings.strength_, 0.0f, 1.0f);
		ImGui::SliderFloat("最大距離", &settings.maxDistance_, 1.0f, 5000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("コーン角", &settings.coneAngle_, 0.0f, 0.5f, "%.3f");
		ImGui::SliderFloat("法線バイアス", &settings.normalBias_, 0.0001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
		ImGui::PopID();
		ImGui::EndDisabled();

		EndSettingsWindow(showSignedDistanceFieldReflectionSettings_);
	}

	void RasterizationPanel::DrawDynamicDiffuseGlobalIlluminationSettingsWindow()
	{
		if (!BeginSettingsWindow("ラスタライゼーション：GI(DDGI)", showDynamicDiffuseGlobalIlluminationSettings_))
		{
			return;
		}

		DynamicDiffuseGlobalIlluminationSettings& settings = context_.viewportContext_.rasterization_.dynamicDiffuseGlobalIllumination_;
		Bool enabled = DrawEnableCheckbox(GraphicsEffect::GlobalIllumination, false, context_.viewportContext_.rasterization_.dynamicDiffuseGlobalIlluminationEnabled_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!enabled);
		ImGui::PushID("DDGI");
		ImGui::SliderFloat("強度", &settings.intensity_, 0.0f, 4.0f, "%.2f");
		ImGui::SliderFloat("プローブ間隔", &settings.probeSpacing_, 0.25f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("ヒステリシス", &settings.hysteresis_, 0.5f, 0.999f, "%.3f");
		ImGui::SliderFloat("法線バイアス", &settings.normalBias_, 0.0f, 1.0f, "%.3f");

		Int raysPerProbe = static_cast<Int>(settings.raysPerProbe_);
		if (ImGui::SliderInt("プローブあたりレイ数", &raysPerProbe, 32, 512))
		{
			settings.raysPerProbe_ = static_cast<Uint32>(raysPerProbe);
		}
		ImGui::PopID();
		ImGui::EndDisabled();

		EndSettingsWindow(showDynamicDiffuseGlobalIlluminationSettings_);
	}
}
