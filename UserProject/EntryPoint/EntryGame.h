#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class World;
}

struct ImGuiContext;

extern "C"
{
	/// [EN] UserProject.dll links its own separate copy of ImGui.lib, so it has its own private ImGui global context — calling any ImGui function from code compiled into this DLL (e.g. reflection-driven inspector drawing) would otherwise crash on a null context. GameModule calls this once, right after loading the DLL, passing Editor.exe's ImGui::GetCurrentContext() so this copy points at the same context.
	/// [JP] UserProject.dll は ImGui.lib を自分専用に静的リンクしているため、ImGuiのグローバルコンテキストも専用の別コピーを持つ — このDLLにコンパイルされたコード(リフレクション駆動のインスペクター描画など)からImGui関数を呼ぶと、そのままでは null コンテキストでクラッシュする。GameModule はDLLロード直後に一度これを呼び、Editor.exe側の ImGui::GetCurrentContext() を渡すことで、このコピーを同じコンテキストに向けさせる。
	__declspec(dllexport) void SC_SetImGuiContext(ImGuiContext* context);

	/// [EN] Called once, right after UserProject.dll is loaded (including reloads). Register/spawn game-owned state here.
	/// [JP] UserProject.dll がロードされた直後(リロード時も含む)に一度だけ呼ばれる。ゲーム側の状態の登録/生成をここで行う。
	__declspec(dllexport) void SC_OnGameLoad(SeedCore::World& world);

	/// [EN] Called once, right before UserProject.dll is unloaded (including reloads). Destroy any Actor/component owned by this module here — the engine cannot safely guess which ones belong to it.
	/// [JP] UserProject.dll がアンロードされる直前(リロード時も含む)に一度だけ呼ばれる。このモジュールが所有する Actor/コンポーネントはここで破棄すること — どれが該当するかはエンジン側では判断できない。
	__declspec(dllexport) void SC_OnGameUnload(SeedCore::World& world);
}
