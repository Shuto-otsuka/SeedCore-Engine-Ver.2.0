#include <Editor/Editor/Panel/VersionPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Resource/TextureLoader.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	VersionPanel::VersionPanel(EditorContext& context)
	{
		TextureLoader* loader = context.worldContext_.loader_->textureLoader_.get();

		std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm local{};
		localtime_s(&local, &t);
		Bool isDaytime = (local.tm_hour >= 6 && local.tm_hour < 18);

		String logoPath = isDaytime ? String("../Runtime/Logo/day_logo.dds") : String("../Runtime/Logo/night_logo.dds");

		Uint index = context.graphicsContext_.descHeap_->AllocateIndex();
		loader->CreateTexture(context.graphicsContext_.device_, context.graphicsContext_.cmdQueue_, context.graphicsContext_.descHeap_->Get(), logoPath, logoResource_, index);
		logoTextureId_ = static_cast<ImTextureID>(context.graphicsContext_.descHeap_->GPUHandle(index).ptr);

		if (logoResource_)
		{
			D3D12_RESOURCE_DESC desc = logoResource_->GetDesc();
			logoWidth_ = static_cast<Float>(desc.Width);
			logoHeight_ = static_cast<Float>(desc.Height);
		}

		if (context.graphicsContext_.adapter_)
		{
			DXGI_ADAPTER_DESC1 adapterDesc{};
			context.graphicsContext_.adapter_->GetDesc1(&adapterDesc);

			Char buf[256]{};
			WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, buf, sizeof(buf), nullptr, nullptr);
			gpuName_ = String(buf);

			Float vramGB = static_cast<Float>(adapterDesc.DedicatedVideoMemory) / (1024.0f * 1024.0f * 1024.0f);
			Char memBuf[64]{};
			std::snprintf(memBuf, sizeof(memBuf), "%.1f GB VRAM", vramGB);
			memoryInfo_ = String(memBuf);
		}

		OSVERSIONINFOEXW osInfo{};
		osInfo.dwOSVersionInfoSize = sizeof(osInfo);

		typedef LONG(WINAPI* RtlGetVersionPtr)(OSVERSIONINFOEXW*);
		HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
		if (ntdll)
		{
			auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"));
			if (rtlGetVersion)
			{
				rtlGetVersion(&osInfo);
			}
		}

		Char osBuf[128]{};
		std::snprintf(osBuf, sizeof(osBuf), "Windows %lu.%lu (Build %lu)", osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber);
		osInfo_ = String(osBuf);
	}

	void VersionPanel::Open()
	{
		show_ = true;
	}

	void VersionPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoResize;

		if (ImGui::Begin("SeedCore Engine について", &show_, flags))
		{
			if (logoTextureId_ && logoWidth_ > 0.0f)
			{
				Float displayWidth = ImGui::GetContentRegionAvail().x;
				Float displayHeight = displayWidth * (logoHeight_ / logoWidth_);
				if (displayHeight > 140.0f)
				{
					displayHeight = 140.0f;
					displayWidth = displayHeight * (logoWidth_ / logoHeight_);
				}
				Float offsetX = (ImGui::GetContentRegionAvail().x - displayWidth) * 0.5f;
				if (offsetX > 0.0f)
				{
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
				}
				ImGui::Image(logoTextureId_, ImVec2(displayWidth, displayHeight));
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::Text("SeedCore Engine");
			ImGui::Text("Version %s", SC_VERTION);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("システム情報");
			ImGui::Spacing();

			ImGui::Text("OS: %s", osInfo_.str().c_str());
			ImGui::Text("GPU: %s", gpuName_.str().c_str());
			ImGui::Text("VRAM: %s", memoryInfo_.str().c_str());

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("コンポーネント");
			ImGui::Spacing();

			if (ImGui::BeginTable("##Components", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("名前", ImGuiTableColumnFlags_WidthStretch, 0.5f);
				ImGui::TableSetupColumn("バージョン", ImGuiTableColumnFlags_WidthStretch, 0.5f);
				ImGui::TableHeadersRow();

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("GraphicsEngine (DirectX12 + DXC)");
				ImGui::TableNextColumn(); ImGui::Text("%s", SC_GRAPHICS_VERTION);

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("PhysicsEngine (JoltPhysics)");
				ImGui::TableNextColumn(); ImGui::Text("%s", SC_PHYSICS_VERTION);

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("AIEngine");
				ImGui::TableNextColumn(); ImGui::Text("%s", SC_AI_VERTION);

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("AudioEngine (CRI ADX)");
				ImGui::TableNextColumn(); ImGui::Text("%s", SC_AUDIO_VERTION);

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("FoundationEngine");
				ImGui::TableNextColumn(); ImGui::Text("%s", SC_FOUNDATION_VERTION);

				ImGui::EndTable();
			}

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				show_ = false;
			}
		}
		ImGui::End();
	}
}
