#include <Editor/Editor/Panel/RaytracingPanel.h>
#include <Editor/Editor/EditorContext.h>

namespace SeedCore
{
	RaytracingPanel::RaytracingPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void RaytracingPanel::DrawMenuItems()
	{
		if (ImGui::MenuItem("影..."))
		{
			showShadowSettings_ = true;
		}

		if (ImGui::MenuItem("AO(アンビエントオクルージョン)..."))
		{
			showAmbientOcclusionSettings_ = true;
		}

		if (ImGui::MenuItem("GI(グローバルイルミネーション)..."))
		{
			showGlobalIlluminationSettings_ = true;
		}

		if (ImGui::MenuItem("反射..."))
		{
			showReflectionSettings_ = true;
		}

		if (ImGui::MenuItem("屈折..."))
		{
			showRefractionSettings_ = true;
		}

		if (ImGui::MenuItem("集光..."))
		{
			showCausticsSettings_ = true;
		}

		if (ImGui::MenuItem("体積光..."))
		{
			showVolumetricLightSettings_ = true;
		}

		if (ImGui::MenuItem("雲..."))
		{
			showVolumetricCloudScapesSettings_ = true;
		}

		if (ImGui::MenuItem("星..."))
		{
			showVolumetricStarSettings_ = true;
		}

		if (ImGui::MenuItem("表面下散乱..."))
		{
			showSubsurfaceScatteringSettings_ = true;
		}
	}

	void RaytracingPanel::DrawWindows()
	{
		DrawShadowSettingsWindow();
		DrawAmbientOcclusionSettingsWindow();
		DrawReflectionSettingsWindow();
		DrawGlobalIlluminationSettingsWindow();
		DrawRefractionSettingsWindow();
		DrawPlaceholderSettingsWindow("レイトレーシング：集光", showCausticsSettings_, context_.viewportContext_.raytracing_.causticsEnabled_);
		DrawVolumetricLightSettingsWindow();
		DrawVolumetricCloudScapesSettingsWindow();
		DrawVolumetricStarSettingsWindow();
		DrawSubsurfaceScatteringSettingsWindow();
	}

	void RaytracingPanel::DrawShadowSettingsWindow()
	{
		if (!showShadowSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("レイトレーシング：影", &showShadowSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.shadowEnabled_);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.shadowEnabled_);

			ImGui::PushID("影");
			ImGui::SliderFloat("強さ", &context_.viewportContext_.raytracing_.shadow_.shadowStrength_, 0.0f, 1.0f);
			ImGui::SliderFloat("最大距離", &context_.viewportContext_.raytracing_.shadow_.rayTMax_, 1.0f, 5000.0f, "%.0f");
			ImGui::SliderFloat("法線バイアス", &context_.viewportContext_.raytracing_.shadow_.normalBias_, 0.0001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("太陽の半径角(ソフトシャドウ)", &context_.viewportContext_.raytracing_.shadow_.sunAngularRadius_, 0.0f, 0.2f, "%.4f");
			ImGui::SliderFloat("Point/Spot/Rectの光源半径", &context_.viewportContext_.raytracing_.shadow_.punctualLightRadius_, 0.0f, 2.0f, "%.3f");
			ImGui::PopID();

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showShadowSettings_ = false;
			}
		}
		ImGui::End();
	}

	void RaytracingPanel::DrawAmbientOcclusionSettingsWindow()
	{
		if (!showAmbientOcclusionSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("レイトレーシング：AO", &showAmbientOcclusionSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.ambientOcclusionEnabled_);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.ambientOcclusionEnabled_);

			ImGui::PushID("AO");
			ImGui::SliderFloat("半径", &context_.viewportContext_.raytracing_.ambientOcclusion_.rayLength_, 0.05f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("強調(指数)", &context_.viewportContext_.raytracing_.ambientOcclusion_.power_, 0.25f, 4.0f, "%.2f");
			ImGui::SliderFloat("法線バイアス", &context_.viewportContext_.raytracing_.ambientOcclusion_.normalBias_, 0.0001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::PopID();

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showAmbientOcclusionSettings_ = false;
			}
		}
		ImGui::End();
	}

	void RaytracingPanel::DrawReflectionSettingsWindow()
	{
		if (!showReflectionSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("レイトレーシング：反射", &showReflectionSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.reflectionEnabled_);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.reflectionEnabled_);

			ImGui::PushID("反射");
			ImGui::SliderFloat("強さ", &context_.viewportContext_.raytracing_.reflection_.strength_, 0.0f, 1.0f);
			ImGui::SliderFloat("最大距離", &context_.viewportContext_.raytracing_.reflection_.rayTMax_, 1.0f, 5000.0f, "%.0f");
			ImGui::SliderFloat("法線バイアス", &context_.viewportContext_.raytracing_.reflection_.normalBias_, 0.0001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::PopID();

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showReflectionSettings_ = false;
			}
		}
		ImGui::End();
	}

	void RaytracingPanel::DrawVolumetricLightSettingsWindow()
	{
		if (!showVolumetricLightSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("レイトレーシング：フォグ / 体積光", &showVolumetricLightSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.volumetricLightEnabled_);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.volumetricLightEnabled_);

			ImGui::SeparatorText("フォグ媒質");

			ImGui::PushID("フォグ媒質");
			ImGui::SliderFloat("密度", &context_.viewportContext_.raytracing_.volumetricLight_.density_, 0.0001f, 0.5f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("吸収", &context_.viewportContext_.raytracing_.volumetricLight_.absorption_, 0.0f, 0.2f, "%.4f");
			ImGui::SliderFloat("高さ減衰", &context_.viewportContext_.raytracing_.volumetricLight_.heightFalloff_, 0.0f, 0.5f, "%.3f");
			ImGui::SliderFloat("基準高さ", &context_.viewportContext_.raytracing_.volumetricLight_.heightReference_, -100.0f, 500.0f, "%.0f");
			ImGui::ColorEdit3("フォグ色", context_.viewportContext_.raytracing_.volumetricLight_.fogAlbedo_);
			ImGui::PopID();

			ImGui::SeparatorText("ゴッドレイ");

			ImGui::PushID("ゴッドレイ");
			ImGui::SliderFloat("強さ", &context_.viewportContext_.raytracing_.volumetricLight_.godrayStrength_, 0.0f, 8.0f, "%.2f");
			ImGui::SliderFloat("前方散乱(HG g)", &context_.viewportContext_.raytracing_.volumetricLight_.scatteringG_, 0.0f, 0.95f, "%.2f");
			ImGui::SliderFloat("シャドウレイ最大距離", &context_.viewportContext_.raytracing_.volumetricLight_.rayTMax_, 10.0f, 10000.0f, "%.0f");

			Bool cloudShadow = context_.viewportContext_.raytracing_.volumetricLight_.cloudShadowEnabled_ != 0;
			if (ImGui::Checkbox("雲の影(雲間からの光芒)", &cloudShadow))
			{
				context_.viewportContext_.raytracing_.volumetricLight_.cloudShadowEnabled_ = cloudShadow ? 1 : 0;
			}

			ImGui::PopID();

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showVolumetricLightSettings_ = false;
			}
		}
		ImGui::End();
	}


	void RaytracingPanel::DrawGlobalIlluminationSettingsWindow()
	{
		if (!showGlobalIlluminationSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("レイトレーシング：GI", &showGlobalIlluminationSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.globalIlluminationEnabled_);

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.globalIlluminationEnabled_);

			ImGui::PushID("GI");
			ImGui::SliderFloat("強度", &context_.viewportContext_.raytracing_.globalIllumination_.intensity_, 0.0f, 4.0f, "%.2f");
			ImGui::SliderFloat("レイ最大距離", &context_.viewportContext_.raytracing_.globalIllumination_.rayTMax_, 50.0f, 20000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
			ImGui::SetItemTooltip("短いと室内のバウンスが消え、長いと全レイがシーン全体の走査コストを払う。");
			ImGui::SliderFloat("法線バイアス", &context_.viewportContext_.raytracing_.globalIllumination_.normalBias_, 0.0f, 1.0f, "%.3f");
			ImGui::PopID();

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showGlobalIlluminationSettings_ = false;
			}
		}
		ImGui::End();
	}

	void RaytracingPanel::DrawRefractionSettingsWindow()
	{
		if (!showRefractionSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("レイトレーシング：屈折", &showRefractionSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.refractionEnabled_);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.refractionEnabled_);

			ImGui::PushID("屈折");
			ImGui::SliderFloat("強さ", &context_.viewportContext_.raytracing_.refraction_.strength_, 0.0f, 1.0f);
			ImGui::SliderFloat("最大距離", &context_.viewportContext_.raytracing_.refraction_.rayTMax_, 1.0f, 5000.0f, "%.0f");
			ImGui::SliderFloat("法線バイアス", &context_.viewportContext_.raytracing_.refraction_.normalBias_, 0.0001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::PopID();

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showRefractionSettings_ = false;
			}
		}
		ImGui::End();
	}

	void RaytracingPanel::DrawCausticsSettingsWindow()
	{

	}

	void RaytracingPanel::DrawVolumetricCloudScapesSettingsWindow()
	{
		if (!showVolumetricCloudScapesSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("レイトレーシング：雲", &showVolumetricCloudScapesSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.volumetricCloudScapesEnabled_);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("※スカイマップ無効時のみ表示されます");

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.volumetricCloudScapesEnabled_);

			ImGui::SeparatorText("雲");

			ImGui::PushID("雲");
			ImGui::SliderFloat("雲量", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.coverage_, 0.0f, 1.0f);
			ImGui::SliderFloat("雲タイプ(左:層雲 右:積雲)", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.cloudType_, 0.0f, 1.0f);
			ImGui::SliderFloat("雨", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.rain_, 0.0f, 1.0f);
			ImGui::SliderFloat("下端高度", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.cloudBottom_, 0.0f, 5000.0f, "%.0f");
			ImGui::SliderFloat("上端高度", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.cloudTop_, 100.0f, 10000.0f, "%.0f");
			ImGui::SliderFloat("密度", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.densityScale_, 0.0001f, 0.5f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("ノイズスケール", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.noiseScale_, 0.0001f, 0.01f, "%.5f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("ディテールスケール", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.detailScale_, 1.0f, 16.0f, "%.1f");
			ImGui::SliderFloat("風速", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.windSpeed_, 0.0f, 0.5f, "%.3f");
			ImGui::ColorEdit3("アルベド", context_.viewportContext_.raytracing_.volumetricCloudScapes_.cloudAlbedo_);
			ImGui::PopID();

			ImGui::SeparatorText("ディテール");

			ImGui::PushID("ディテール");
			ImGui::SliderFloat("侵食の強さ", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.detailStrength_, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("ディテール風速比", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.detailWindSpeed_, 0.0f, 2.0f, "%.2f");
			ImGui::PopID();

			ImGui::SeparatorText("天候(大きな雲の塊)");

			ImGui::PushID("天候(大きな雲の塊)");
			ImGui::SliderFloat("weather スケール", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.weatherScale_, 0.005f, 0.5f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("weather 強度", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.weatherAmount_, 0.0f, 1.0f, "%.2f");
			ImGui::TextDisabled("※0 にすると空全体で雲量が一定になります");
			ImGui::PopID();

			ImGui::SeparatorText("散乱");

			ImGui::PushID("散乱");
			ImGui::SliderFloat("前方散乱(HG g)", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.scatteringG_, -0.9f, 0.9f, "%.2f");
			ImGui::SliderFloat("後方ローブ", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.phaseBackward_, 0.0f, 0.9f, "%.2f");
			ImGui::SliderFloat("ローブブレンド", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.phaseBlend_, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("powder(日向側の締まり)", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.powderStrength_, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("環境光", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.ambientStrength_, 0.0f, 2.0f, "%.2f");

			Int multiScatterOctaves = static_cast<Int>(context_.viewportContext_.raytracing_.volumetricCloudScapes_.multiScatterOctaves_);
			if (ImGui::SliderInt("多重散乱オクターブ", &multiScatterOctaves, 1, 8))
			{
				context_.viewportContext_.raytracing_.volumetricCloudScapes_.multiScatterOctaves_ = static_cast<Uint32>(multiScatterOctaves);
			}

			ImGui::SliderFloat("多重散乱：消散減衰", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.multiScatterAttenuation_, 0.1f, 0.95f, "%.2f");
			ImGui::SliderFloat("多重散乱：寄与減衰", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.multiScatterContribution_, 0.1f, 0.95f, "%.2f");
			ImGui::SetItemTooltip("雲の明るさを決める本体。オクターブ数との総和が\n""1/(1-この値) に近づく。総和3前後で日向の白い面と同じ明るさ。");
			ImGui::SliderFloat("多重散乱：位相減衰", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.multiScatterEccentricity_, 0.1f, 0.95f, "%.2f");
			ImGui::PopID();

			ImGui::SeparatorText("層の形状と遠景");

			ImGui::PushID("層の形状と遠景");
			ImGui::SliderFloat("惑星半径", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.planetRadius_, 100000.0f, 40000000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
			ImGui::SetItemTooltip("雲層を球殻として扱う半径。小さくすると雲が早く地平線へ落ちる。");
			ImGui::SliderFloat("マーチ上限距離", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.maxMarchDistance_, 20000.0f, 400000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("LOD 開始距離", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.lodDistance_, 2000.0f, 100000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("空気遠近の濃さ", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.aerialDensity_, 0.0f, 0.0002f, "%.6f", ImGuiSliderFlags_Logarithmic);
			ImGui::PopID();

			ImGui::SeparatorText("サンプリング");

			ImGui::PushID("サンプリング");
			Int stepCount = static_cast<Int>(context_.viewportContext_.raytracing_.volumetricCloudScapes_.stepCount_);
			if (ImGui::SliderInt("ステップ数", &stepCount, 8, 256))
			{
				context_.viewportContext_.raytracing_.volumetricCloudScapes_.stepCount_ = static_cast<Uint32>(stepCount);
			}

			Int lightStepCount = static_cast<Int>(context_.viewportContext_.raytracing_.volumetricCloudScapes_.lightStepCount_);
			if (ImGui::SliderInt("ライトステップ数", &lightStepCount, 1, 16))
			{
				context_.viewportContext_.raytracing_.volumetricCloudScapes_.lightStepCount_ = static_cast<Uint32>(lightStepCount);
			}

			ImGui::SliderFloat("テンポラルジッタ", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.temporalJitter_, 0.0f, 1.0f, "%.2f");
			ImGui::SetItemTooltip("0=フレーム非依存ディザ(安定)。上げると毎フレーム変動するので TAA/DLSS 前提。");
			ImGui::PopID();

			ImGui::SeparatorText("空と太陽");

			ImGui::PushID("空と太陽");
			ImGui::ColorEdit3("天頂色(空の青さ)", context_.viewportContext_.raytracing_.volumetricCloudScapes_.skyZenithColor_);
			ImGui::ColorEdit3("地平線色", context_.viewportContext_.raytracing_.volumetricCloudScapes_.skyHorizonColor_);
			ImGui::ColorEdit3("地面色", context_.viewportContext_.raytracing_.volumetricCloudScapes_.groundColor_);
			ImGui::SliderFloat("空の明るさ", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.skyBrightness_, 0.0f, 4.0f, "%.2f");
			ImGui::SliderFloat("太陽の視半径", &context_.viewportContext_.raytracing_.volumetricCloudScapes_.sunSize_, 0.005f, 0.2f, "%.3f");
			ImGui::PopID();

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showVolumetricCloudScapesSettings_ = false;
			}
		}
		ImGui::End();
	}

	void RaytracingPanel::DrawVolumetricStarSettingsWindow()
	{
		if (!showVolumetricStarSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("レイトレーシング：星", &showVolumetricStarSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.volumetricStarEnabled_);
			ImGui::TextDisabled("※グラフィックス→環境の DaySystem が有効で夜になるほど強く見えます");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.volumetricStarEnabled_);

			ImGui::SeparatorText("星空");

			ImGui::PushID("星空");
			ImGui::SliderFloat("グリッドの角度サイズ", &context_.viewportContext_.raytracing_.volumetricStar_.cellSize_, 0.02f, 0.5f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("密度", &context_.viewportContext_.raytracing_.volumetricStar_.density_, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("明るさ", &context_.viewportContext_.raytracing_.volumetricStar_.brightness_, 0.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("瞬き速度", &context_.viewportContext_.raytracing_.volumetricStar_.twinkleSpeed_, 0.0f, 10.0f, "%.2f");
			ImGui::ColorEdit3("色", context_.viewportContext_.raytracing_.volumetricStar_.color_);
			ImGui::SliderFloat("サイズ(最小)", &context_.viewportContext_.raytracing_.volumetricStar_.sizeMin_, 0.005f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("サイズ(最大)", &context_.viewportContext_.raytracing_.volumetricStar_.sizeMax_, 0.005f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("Glow強さ", &context_.viewportContext_.raytracing_.volumetricStar_.glowIntensity_, 0.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Glow減衰(大きいほど締まる)", &context_.viewportContext_.raytracing_.volumetricStar_.glowFalloff_, 10.0f, 400.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
			ImGui::PopID();

			ImGui::SeparatorText("流れ星");

			ImGui::PushID("流れ星");
			ImGui::SliderFloat("発生確率(1秒あたり)", &context_.viewportContext_.raytracing_.volumetricStar_.shootingStarChancePerSecond_, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("明るさ", &context_.viewportContext_.raytracing_.volumetricStar_.shootingStarBrightness_, 0.0f, 10.0f, "%.2f");
			ImGui::SliderFloat("筋の太さ", &context_.viewportContext_.raytracing_.volumetricStar_.shootingStarWidth_, 0.0002f, 0.02f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::PopID();

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showVolumetricStarSettings_ = false;
			}
		}
		ImGui::End();
	}

	void RaytracingPanel::DrawSubsurfaceScatteringSettingsWindow()
	{
		if (!showSubsurfaceScatteringSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("レイトレーシング：表面下散乱", &showSubsurfaceScatteringSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.subsurfaceScatteringEnabled_);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.subsurfaceScatteringEnabled_);

			ImGui::PushID("表面下散乱");
			ImGui::SliderFloat("散乱距離", &context_.viewportContext_.raytracing_.subsurfaceScattering_.scatterDistance_, 0.01f, 5.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("強さ", &context_.viewportContext_.raytracing_.subsurfaceScattering_.strength_, 0.0f, 4.0f, "%.2f");
			ImGui::SliderFloat("最大厚み", &context_.viewportContext_.raytracing_.subsurfaceScattering_.rayTMax_, 0.1f, 50.0f, "%.1f");
			ImGui::SliderFloat("厚みバイアス", &context_.viewportContext_.raytracing_.subsurfaceScattering_.thicknessBias_, 0.0001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::ColorEdit3("透光色", context_.viewportContext_.raytracing_.subsurfaceScattering_.subsurfaceColor_);
			ImGui::PopID();

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showSubsurfaceScatteringSettings_ = false;
			}
		}
		ImGui::End();
	}

	void RaytracingPanel::DrawPlaceholderSettingsWindow(const Char* title, Bool& show, Bool& enabled)
	{
		if (!show)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin(title, &show, flags))
		{
			ImGui::Checkbox("有効", &enabled);

			ImGui::Spacing();
			ImGui::TextDisabled("未実装(器のみ)");

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				show = false;
			}
		}
		ImGui::End();
	}
}
