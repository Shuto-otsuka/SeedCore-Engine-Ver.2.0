#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] Which view's accumulation chain a raytraced-effect dispatch call
	///      targets. Every screen-space RT signal (shadow, AO, ...) is
	///      per-camera, so the editor and game views each own an independent
	///      history/write ping-pong pair — sharing one chain makes each view's
	///      history be the OTHER view's result, which the denoiser then
	///      rejects every frame (visible as raw noise).
	/// [JP] レイトレエフェクトのディスパッチがどのビューの蓄積チェーンを対象に
	///      するか。スクリーンスペースの RT 信号(影、AO、...)はカメラ依存
	///      なので、エディタ/ゲームビューがそれぞれ独立した history/write の
	///      ピンポンペアを持つ。1本を共有すると各ビューの history が「もう
	///      片方のビューの結果」になり、デノイザが毎フレーム棄却して生ノイズが
	///      見える。
	enum class RaytracingView : Uint32
	{
		Editor = 0,
		Game = 1,
	};
}
