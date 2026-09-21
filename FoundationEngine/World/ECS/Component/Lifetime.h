#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that destroys its owning actor after a fixed time.
	* Holds only the configured duration; the countdown itself is
	* tracked per entity by LifetimeSystem, which decrements it every
	* frame and records the actor's destruction on the frame's
	* CommandBuffer once it reaches zero. Pairs with Spawner for "spawn,
	* live briefly, disappear" gameplay.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 一定時間後に所有アクターを破棄するコンポーネント。設定値である
	* 生存時間のみを保持し、カウントダウン自体は LifetimeSystem が
	* エンティティごとに追跡する（毎フレーム減算し、0 に達したら
	* そのフレームの CommandBuffer へアクターの破棄を記録する）。
	* 「生成 → 短時間だけ存在 → 消滅」の
	* ゲームプレイで Spawner と対になる。
	*/
	struct Lifetime
	{
		/// [EN] Seconds the actor lives after the countdown starts, before it is destroyed.
		/// [JP] カウントダウン開始から破棄されるまで、アクターが存在する秒数。
		SC_REFLECTION_CLAMPED_EX("生存時間(秒)", 0.05f, 3600.0f)
		Float duration_ = 5.0f;
	};
	REGISTER_COMPONENT(Lifetime, "Gameplay");
}
