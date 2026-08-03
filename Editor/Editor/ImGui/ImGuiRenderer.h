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

	private:
		Bool FontConfig(ImGuiIO& io);
		void StyleConfig();

	private:
		ResourcePtr<DescriptorHeap> descHeap_;

		Float fontScale_ = 1.0f;
	};
}