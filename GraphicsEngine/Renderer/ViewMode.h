#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] Editor view / buffer visualization mode. Shared by the editor UI
	///      (menu) and the renderer (deferred composite switch), so the enum
	///      value is the single source of truth passed to the shader.
	/// [JP] エディタの表示 / バッファ可視化モード。エディタUI（メニュー）と
	///      描画側（デファード合成の switch）で共有し、この enum 値をそのまま
	///      シェーダへ渡す唯一の真実とする。
	enum class ViewMode : Uint32
	{
		Lit = 0,      // ライティングあり（通常）
		Unlit,        // ライティングなし（アルベド直出し）
		Wireframe,    // ワイヤーフレーム
		Depth,        // 深度
		Meshlet,      // メッシュレット

		Normal,       // 法線
		Roughness,    // ラフネス
		Metallic,     // メタルネス
		Emissive,     // エミッシブ
		Velocity,     // モーションベクター
	};
}
