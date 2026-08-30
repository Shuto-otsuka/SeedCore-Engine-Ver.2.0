#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Drives every Lifetime component in a World: counts each holding
	* actor's remaining time down from Lifetime::duration_ and destroys
	* the actor once it reaches zero. Update must be called every frame
	* to advance the countdowns. The remaining time is tracked here per
	* entity rather than on Lifetime itself, since Lifetime only holds
	* reflected/serialized configuration. Pairs with SpawnerSystem,
	* whose spawned instances rely on the same "live briefly, then
	* disappear" behavior.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* World 内の全 Lifetime コンポーネントを駆動する: 各保持アクターの
	* 残り時間を Lifetime::duration_ からカウントダウンし、0 に達したら
	* そのアクターを破棄する。カウントダウンを進めるには Update を毎
	* フレーム呼び出す必要がある。残り時間は Lifetime 自体ではなく
	* こちらでエンティティごとに追跡する。Lifetime はリフレクション/
	* シリアライズされる設定値のみを保持するため。「短時間だけ存在して
	* 消える」挙動を共有する SpawnerSystem と対になる。
	*/
	class LifetimeSystem
	{
	public:
		/**
		* [EN]
		* Advances every Lifetime component's countdown by deltaTime and
		* destroys any actor whose remaining time has reached zero. A
		* holding actor seen for the first time starts its countdown at
		* Lifetime::duration_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全 Lifetime コンポーネントのカウントダウンを deltaTime だけ
		* 進め、残り時間が 0 に達したアクターを破棄する。初めて見つかった
		* 保持アクターは Lifetime::duration_ からカウントダウンを開始する。
		*/
		void Update(World& world, Float deltaTime);

		/**
		* [EN]
		* Forgets every Lifetime's countdown without touching the actual
		* Actors - call this whenever the World itself was reset out from
		* under this system (e.g. WorldSnapshot::Restore on Editor Stop),
		* so stale EntityIDs from the previous session aren't carried
		* forward into the next Play.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全 Lifetime のカウントダウンを、実際の Actor には触れずに
		* 忘れる - このシステムの下で World 自体がリセットされた時
		* (例: Editor の Stop 時の WorldSnapshot::Restore)に呼ぶこと。
		* 前回セッションの古い EntityID を次の Play へ持ち越さないため。
		*/
		void Reset();

	private:
		/// [EN] Remaining time before destruction for every entity currently holding a Lifetime, keyed by EntityID.
		/// [JP] 現在 Lifetime を持つ全エンティティの、破棄までの残り時間。EntityID をキーとする。
		FlatMap<EntityID, Float> remaining_;
	};
}
