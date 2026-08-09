#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <GraphicsEngine/Graphics.h>

namespace SeedCore
{
	Bool ImGuiRenderer::Initialize(HWND hwnd, ID3D12Device* device, Int numberFramesInFlight)
	{
		descHeap_ = MakePtr<DescriptorHeap>();
		if (!descHeap_->Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8192, true))
		{
			return false;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		Graphics::SetImGuiContext(ImGui::GetCurrentContext());

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		if (!ImGui_ImplWin32_Init(hwnd))
		{
			return false;
		}

		if (!ImGui_ImplDX12_Init(device, numberFramesInFlight, DXGI_FORMAT_R8G8B8A8_UNORM, descHeap_->Get(), descHeap_->Get()->GetCPUDescriptorHandleForHeapStart(), descHeap_->Get()->GetGPUDescriptorHandleForHeapStart()))
		{
			return false;
		}

		if (!FontConfig(io))
		{
			return false;
		}
		StyleConfig();

		if (!ImGui_ImplDX12_CreateDeviceObjects())
		{
			return false;
		}

		descHeap_->AllocateIndex();

		return true;
	}

	void ImGuiRenderer::FontScale(Float scale)
	{
		fontScale_ = scale;
		ImGui::GetIO().FontGlobalScale = fontScale_;
	}

	Float ImGuiRenderer::FontScale()const
	{
		return fontScale_;
	}

	void ImGuiRenderer::NewFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiRenderer::ClearInputKeys()
	{
		if (ImGui::GetCurrentContext() != nullptr)
		{
			ImGui::GetIO().ClearInputKeys();
		}
	}

	void ImGuiRenderer::DockSpaceBegin(Float topOffset)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + topOffset));
		ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - topOffset));
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGui::Begin("MainDockSpace", nullptr, windowFlags);
		ImGui::PopStyleVar(3);

		dockSpaceID_ = ImGui::GetID("ScDockSpace");
		ImGui::DockSpace(dockSpaceID_, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
	}

	void ImGuiRenderer::DockSpaceEnd()
	{
		ImGui::End();
	}

	void ImGuiRenderer::Render(ID3D12GraphicsCommandList* cmdList)
	{
		ImGui::Render();
		ID3D12DescriptorHeap* descHeaps[] = { descHeap_->Get() };
		cmdList->SetDescriptorHeaps(1, descHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void ImGuiRenderer::Finalize()
	{
		if (ImGui::GetCurrentContext()) 
		{
			ImGuiIO& io = ImGui::GetIO();
			io.Fonts->Clear();
		}

		ImGui_ImplDX12_InvalidateDeviceObjects();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		if (descHeap_)
		{
			descHeap_.reset();
			descHeap_ = nullptr;
		}
	}

	DescriptorHeap* ImGuiRenderer::GetDescriptorHeap()const
	{
		return descHeap_.get();
	}

	LRESULT ImGuiRenderer::HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (ImGui::GetCurrentContext() != nullptr)
		{
			return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
		}
		return 0;
	};

	Bool ImGuiRenderer::FontConfig(ImGuiIO& io)
	{
		ImFontConfig fontConfig{};
		fontConfig.OversampleH = 1;
		fontConfig.OversampleV = 1;
		fontConfig.PixelSnapH = false;

		const ImWchar* japaneseFullRange = io.Fonts->GetGlyphRangesChineseFull();
		io.Fonts->AddFontFromFileTTF("../External/ImGui/Font/NotoSansJP-Regular.ttf", 22.0f, &fontConfig, japaneseFullRange);
		return io.Fonts->Build();
	}

	void ImGuiRenderer::StyleConfig()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		style.WindowPadding = ImVec2(5, 5);
		style.FramePadding = ImVec2(7, 5);
		style.CellPadding = ImVec2(4, 2);
		style.ItemSpacing = ImVec2(7, 5);
		style.ItemInnerSpacing = ImVec2(4, 4);
		style.TouchExtraPadding = ImVec2(0, 0);
		style.IndentSpacing = 11;
		style.ScrollbarSize = 13;
		style.GrabMinSize = 9;

		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
		style.WindowRounding = 7.0f;
		style.ChildRounding = 5.0f;
		style.FrameRounding = 5.0f;
		style.PopupRounding = 5.0f;
		style.ScrollbarRounding = 8.0f;
		style.GrabRounding = 2.0f;
		style.TabRounding = 5.0f;

		colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.22f, 0.22f, 0.24f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.20f, 0.98f);

		colors[ImGuiCol_Border] = ImVec4(0.50f, 0.40f, 0.70f, 0.50f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

		colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.35f, 0.25f, 0.55f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.17f, 0.80f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);

		colors[ImGuiCol_FrameBg] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.45f, 0.35f, 0.65f, 0.45f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.45f, 0.35f, 0.65f, 0.60f);

		colors[ImGuiCol_Tab] = ImVec4(0.25f, 0.22f, 0.32f, 0.80f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.60f, 0.45f, 0.90f, 0.85f);
		colors[ImGuiCol_TabActive] = ImVec4(0.50f, 0.35f, 0.75f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.20f, 0.18f, 0.24f, 0.98f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.30f, 0.25f, 0.40f, 1.00f);

		colors[ImGuiCol_Button] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.44f, 0.24f, 0.66f, 0.80f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.44f, 0.24f, 0.66f, 1.00f);

		colors[ImGuiCol_Header] = ImVec4(0.44f, 0.24f, 0.66f, 0.40f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.24f, 0.66f, 0.70f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.44f, 0.24f, 0.66f, 1.00f);

		colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.22f, 0.55f, 0.50f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.55f, 0.35f, 0.85f, 0.78f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.55f, 0.35f, 0.85f, 1.00f);

		colors[ImGuiCol_CheckMark] = ImVec4(0.66f, 0.44f, 0.99f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.44f, 0.24f, 0.66f, 0.80f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.66f, 0.44f, 0.99f, 1.00f);

		colors[ImGuiCol_DockingPreview] = ImVec4(0.44f, 0.24f, 0.66f, 0.70f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.44f, 0.24f, 0.66f, 0.20f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.24f, 0.66f, 0.67f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.44f, 0.24f, 0.66f, 0.95f);
	}
}