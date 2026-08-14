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

		/// [EN] Ray-traced signals, tapped before and after their denoiser. Each
		///      effect traces into a raw buffer and then runs a denoise chain
		///      over it, and a fault in either stage looks identical in the
		///      final image - so both taps are exposed. Comparing the raw and
		///      denoised view of one effect says immediately which of the two
		///      stages lost the signal.
		/// [JP] レイトレ信号を、デノイズの前と後の両方で覗くモード。各エフェクトは
		///      生バッファへトレースしてからデノイズチェーンを回すが、どちらの段が
		///      壊れても最終画では同じに見える。そのため両方の口を出す。同じ
		///      エフェクトの生とデノイズ後を見比べれば、どちらの段で信号が失われた
		///      のかが即座に分かる。
		ReflectionRaw,               // 反射（生・1spp）
		ReflectionDenoised,          // 反射（デノイズ後）
		GlobalIlluminationRaw,       // グローバルイルミネーション（生・1spp）
		GlobalIlluminationDenoised,  // グローバルイルミネーション（デノイズ後）
		ShadowRaw,                   // シャドウ（生・1spp）
		ShadowDenoised,              // シャドウ（デノイズ後）
		AmbientOcclusionRaw,         // アンビエントオクルージョン（生・1spp）
		AmbientOcclusionDenoised,    // アンビエントオクルージョン（デノイズ後）
	};
}
