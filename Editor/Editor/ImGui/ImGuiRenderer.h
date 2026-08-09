#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>

namespace SeedCore
{
	class ImGuiRenderer
	{
	public:
		ImGuiRenderer() = default;
		~ImGuiRenderer() = default;

		Bool Initialize(HWND hwnd, ID3D12Device* device, Int numberFramesInFlight);

		void NewFrame();

		void Render(ID3D12GraphicsCommandList* cmdList);

		void DockSpaceBegin(Float topOffset = 0.0f);

		void DockSpaceEnd();

		void Finalize();

		static LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		/**
		* [EN] Clears ImGui's keyboard state. Called on window focus gain to
		*      release modifier keys left stuck "down" when a modal dialog
		*      swallowed their KeyUp (which otherwise breaks wheel scrolling).
		* [JP] ImGui のキーボード状態をクリアする。モーダルダイアログが KeyUp を
		*      吸って修飾キーが固着した際、フォーカス復帰時に呼んで解放する
		*      （放置するとホイールスクロールが壊れる）。
		*/
		static void ClearInputKeys();

		DescriptorHeap* GetDescriptorHeap()const;

		void FontScale(Float scale);

		Float FontScale()const;

		/**
		* [EN] Returns the ImGuiID of the main dockspace ("ScDockSpace"), as
		*      resolved inside DockSpaceBegin(). ImGui::GetID(const char*)
		*      hashes using the current window's ID stack as a seed, so any
		*      caller outside DockSpaceBegin() re-computing ImGui::GetID(
		*      "ScDockSpace") on its own gets a DIFFERENT ID than the real
		*      dockspace node — panels that need to force-dock into it (e.g.
		*      via ImGui::DockBuilderDockWindow) must use this getter instead.
		* [JP] DockSpaceBegin() 内で解決されたメインドックスペース
		*      ("ScDockSpace") の ImGuiID を返す。ImGui::GetID(const char*) は
		*      カレントウィンドウの ID スタックをシードにハッシュするため、
		*      DockSpaceBegin() の外で独自に ImGui::GetID("ScDockSpace") を
		*      呼んでも実際のドックスペースノードとは別の ID になってしまう
		*      ―― ImGui::DockBuilderDockWindow などで強制ドックしたいパネルは
		*      このゲッター経由で取得すること。
		*/
		[[nodiscard]] ImGuiID GetDockSpaceID()const { return dockSpaceID_; }

	private:
		Bool FontConfig(ImGuiIO& io);
		void StyleConfig();

	private:
		ResourcePtr<DescriptorHeap> descHeap_;

		Float fontScale_ = 1.0f;

		/// [EN] ID of the main dockspace, resolved each frame in DockSpaceBegin().
		/// [JP] メインドックスペースのID。毎フレーム DockSpaceBegin() 内で解決される。
		ImGuiID dockSpaceID_ = 0;
	};
}