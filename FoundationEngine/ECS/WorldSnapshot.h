#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Captures a byte-level snapshot of every Actor's component data in
	* a World, and can later restore that data back onto the same
	* actors (e.g. to reset play-mode changes in the editor back to the
	* pre-play state).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* World 内の全 Actor のコンポーネントデータを、バイト単位の
	* スナップショットとして保存し、後で同じ actor 群へ復元できる
	* クラス（例: エディタでプレイモードの変更をプレイ前の状態へ
	* リセットするために使う）。
	*/
	class SEEDCORE_API WorldSnapshot
	{
	public:
		/**
		* [EN]
		* Clears any existing snapshot, then captures a copy of every
		* Actor's component data currently in world — both archetype-stored
		* (via each actor's layout) and sparse-set-stored (Rigidbody,
		* Softbody, Weather, ...; walked via the full component registry,
		* since sparse-set storage has no per-entity layout list).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 既存のスナップショットをクリアし、world 内の現在の全 Actor の
		* コンポーネントデータのコピーを取得する — アーキタイプ格納
		* （各 actor のレイアウト経由）とスパースセット格納
		* （Rigidbody、Softbody、Weather 等; スパースセット格納には
		* エンティティごとのレイアウト一覧が無いため、コンポーネント
		* レジストリ全体を走査する）の両方。
		*/
		void Capture(World& world);

		/**
		* [EN]
		* If a snapshot has been captured, copies its component data
		* back onto world's actors (matched by index) and resets each
		* ComponentBase's lifecycle state, then clears the snapshot.
		* Does nothing if no snapshot exists.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スナップショットが取得済みであれば、そのコンポーネントデータを
		* （インデックスで対応付けた）world の actor 群へコピーし戻し、
		* 各 ComponentBase のライフサイクル状態をリセットした上で、
		* スナップショットをクリアする。スナップショットが存在しなければ
		* 何もしない。
		*/
		void Restore(World& world);

		/**
		* [EN]
		* Returns whether a snapshot is currently held.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スナップショットを現在保持しているかどうかを返す。
		*/
		Bool HasData()const;

		/**
		* [EN]
		* Destructs and discards all captured component data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 取得済みの全コンポーネントデータを破棄・削除する。
		*/
		void Clear();

	private:
		/**
		* [EN]
		* A single captured component's raw byte data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 取得済みの単一コンポーネントの、生のバイトデータ。
		*/
		struct ComponentData
		{
			/// [EN] The component's registered ComponentID.
			/// [JP] そのコンポーネントの登録済み ComponentID。
			ComponentID id_;

			/// [EN] The component's raw copied bytes.
			/// [JP] コピーされたコンポーネントの生バイト列。
			DynamicArray<Uint8> data_;
		};

		/**
		* [EN]
		* A single captured Actor's data: its name, every component's
		* captured bytes, and which of them derive from ComponentBase.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 取得済みの単一 Actor のデータ: その名前、全コンポーネントの
		* 取得済みバイト列、およびそのうちどれが ComponentBase から
		* 派生しているか。
		*/
		struct ActorData
		{
			/// [EN] The actor's display name at capture time.
			/// [JP] 取得時点での actor の表示名。
			String name_;

			/// [EN] Captured byte data for every component the actor had.
			/// [JP] actor が持っていた全コンポーネントの、取得済みバイトデータ。
			DynamicArray<ComponentData> components_;

			/// [EN] IDs of the components that derive from ComponentBase, for resetting lifecycle state on restore.
			/// [JP] ComponentBase から派生するコンポーネントの ID 一覧。復元時にライフサイクル状態をリセットするために使う。
			DynamicArray<ComponentID> componentBaseIDs_;

			/// [EN] Index of the actor's parent within actors_ at capture time, or -1 if it had none.
			/// [JP] 取得時点における、actors_ 内でのその actor の親のインデックス。親が無ければ -1。
			Int parentIndex_ = -1;
		};

		/// [EN] Every captured actor's data.
		/// [JP] 取得済みの全 actor のデータ。
		DynamicArray<ActorData> actors_;

		/// [EN] Whether a snapshot has been captured and not yet restored/cleared.
		/// [JP] スナップショットが取得済みで、まだ復元/クリアされていないかどうか。
		Bool hasData_ = false;
	};
}
