#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	class World;
	class ResourceCache;

	/**
	* [EN]
	* Records structural World changes (entity create/destroy, prefab
	* spawn, component add/remove) as a list to be replayed later, in
	* record order, by Flush. Systems issue changes here instead of
	* mutating the World directly so structural edits never happen while
	* a Query::ForEach is walking chunks - the engine flushes the buffer
	* at fixed points between systems. Create/SpawnPrefab hand back a
	* provisional EntityID; after Flush, Resolved maps it to the real
	* EntityID so a system can keep tracking what it spawned across
	* frames. The design is per-worker + merge, but a single buffer is
	* wired for now while the built-in systems run serially.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* World への構造変更（エンティティの生成/破棄、プレハブ生成、
	* コンポーネントの追加/削除）を、後で Flush が記録順に再生する
	* リストとして記録する。システムは World を直接変更する代わりに
	* ここへ変更を積むことで、Query::ForEach がチャンクを走査している
	* 最中に構造編集が起きないようにする - エンジンがシステムの合間の
	* 決まった地点でバッファを flush する。Create / SpawnPrefab は
	* 暫定 EntityID を返す。Flush 後、Resolved がそれを実際の EntityID へ
	* 対応付けるため、システムは生成したものをフレームをまたいで追跡
	* できる。設計はワーカーごと + マージだが、組み込みシステムが直列に
	* 走る現状では単一バッファのみ配線している。
	*/
	class SEEDCORE_API CommandBuffer
	{
	public:
		/**
		* [EN]
		* Default constructor: starts with an empty command list.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 空のコマンドリストから開始する。
		*/
		CommandBuffer() = default;

		/**
		* [EN]
		* Destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		~CommandBuffer() = default;

		/**
		* [EN]
		* Records the creation of a bare (componentless) entity and
		* returns a provisional EntityID that stands in for it until
		* Flush; pass that ID to this buffer's AddComponent/DestroyEntity
		* to target the not-yet-created entity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コンポーネントを持たないエンティティの生成を記録し、Flush まで
		* それを代理する暫定 EntityID を返す。まだ生成されていない
		* エンティティを対象にするには、その ID をこのバッファの
		* AddComponent / DestroyEntity へ渡す。
		*/
		EntityID CreateEntity();

		/**
		* [EN]
		* Records a deferred prefab instantiation as a root actor (or a
		* child of parent when valid), placing its root Position at
		* position. Returns a provisional EntityID for the root; use
		* Resolved after Flush to recover the real one.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレハブのインスタンス化を、ルート actor（parent が有効なら
		* その子）として遅延記録し、そのルートの Position を position に
		* 配置する。ルートの暫定 EntityID を返す。実際の ID は Flush 後に
		* Resolved で取得する。
		*/
		EntityID SpawnPrefab(Uint32 assetID, const Vector3& position, EntityID parent = EntityID{});

		/**
		* [EN]
		* Records the destruction of id (an actor is destroyed through
		* World::DestroyActor, a bare entity through World::DestroyEntity).
		* id may be a provisional ID from this buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id の破棄を記録する（actor は World::DestroyActor、素の
		* エンティティは World::DestroyEntity 経由で破棄される）。id は
		* このバッファの暫定 ID でもよい。
		*/
		void DestroyEntity(EntityID id);

		/**
		* [EN]
		* Records adding a copy of value as id's T component. id may be a
		* provisional ID from this buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value のコピーを id の T コンポーネントとして追加することを
		* 記録する。id はこのバッファの暫定 ID でもよい。
		*/
		template<typename T>
		void AddComponent(EntityID id, const T& value);

		/**
		* [EN]
		* Type-erased AddComponent: records a copy of the component at
		* source, registered under componentID, for id.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 型消去版 AddComponent: componentID に登録されたコンポーネントを
		* source からコピーして id 用に記録する。
		*/
		void AddComponent(EntityID id, ComponentID componentID, const void* source);

		/**
		* [EN]
		* Records removing id's component registered under componentID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* componentID に登録された id のコンポーネントの削除を記録する。
		*/
		void RemoveComponent(EntityID id, ComponentID componentID);

		/**
		* [EN]
		* Returns whether no commands are recorded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コマンドが1つも記録されていないかどうかを返す。
		*/
		Bool Empty()const;

		/**
		* [EN]
		* Replays every recorded command against world in record order,
		* then clears the command list (keeping the provisional-to-real
		* map so Resolved works until the next Flush).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 記録された全コマンドを world に対して記録順で再生し、コマンド
		* リストをクリアする（次の Flush まで Resolved が使えるよう、
		* 暫定→実際の対応表は残す）。
		*/
		void Flush(World& world, ResourceCache& cache);

		/**
		* [EN]
		* Returns the real EntityID the given provisional ID resolved to
		* in the last Flush, or an invalid EntityID if it is unknown (or
		* not provisional).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定した暫定 ID が直近の Flush で解決された実際の EntityID を
		* 返す。不明な場合（または暫定 ID でない場合）は無効な EntityID を
		* 返す。
		*/
		EntityID Resolved(EntityID provisional)const;

		/**
		* [EN]
		* Drops every recorded command and the provisional-to-real map.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 記録された全コマンドと、暫定→実際の対応表を破棄する。
		*/
		void Clear();

		/**
		* [EN]
		* Returns whether id is a provisional ID handed out by a
		* CommandBuffer (rather than a real World EntityID).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id が（実際の World の EntityID ではなく）CommandBuffer が発行した
		* 暫定 ID かどうかを返す。
		*/
		static Bool IsProvisional(EntityID id);

	private:
		/// [EN] Generation value that marks an EntityID as a CommandBuffer provisional handle; no live entity ever reaches it.
		/// [JP] EntityID を CommandBuffer の暫定ハンドルとして印付ける世代値。生存中のエンティティがこの値に達することはない。
		static constexpr Uint32 ProvisionalGeneration = 0xFFFFFFFFu;

		/// [EN] Discriminates what a recorded Command does.
		/// [JP] 記録された Command が何をするかを区別する。
		enum class Kind : Uint8
		{
			CreateEntity,
			SpawnPrefab,
			DestroyEntity,
			AddComponent,
			RemoveComponent
		};

		/// [EN] One recorded structural change; see Kind for which fields apply.
		/// [JP] 記録された1つの構造変更。どのフィールドが有効かは Kind を参照。
		struct Command
		{
			/// [EN] What this command does.
			/// [JP] このコマンドが行う操作。
			Kind kind_ = Kind::CreateEntity;

			/// [EN] Target entity (real or provisional); the created/spawned entity for CreateEntity/SpawnPrefab.
			/// [JP] 対象エンティティ（実際または暫定）。CreateEntity/SpawnPrefab では生成されるエンティティ。
			EntityID target_;

			/// [EN] SpawnPrefab: parent entity, or invalid for a root spawn.
			/// [JP] SpawnPrefab: 親エンティティ。ルート生成なら無効。
			EntityID parent_;

			/// [EN] Add/RemoveComponent: which component type.
			/// [JP] Add/RemoveComponent: どのコンポーネント型か。
			ComponentID componentID_ = nullptr;

			/// [EN] SpawnPrefab: the .prefab asset ID to instantiate.
			/// [JP] SpawnPrefab: インスタンス化する .prefab アセット ID。
			Uint32 assetID_ = 0;

			/// [EN] SpawnPrefab: world position to write into the spawned root's Position.
			/// [JP] SpawnPrefab: 生成されたルートの Position に書き込むワールド位置。
			Vector3 position_;

			/// [EN] AddComponent: index of the first blob_ cell holding the recorded component copy.
			/// [JP] AddComponent: 記録されたコンポーネントのコピーを保持する blob_ の先頭セルのインデックス。
			Size dataOffset_ = 0;
		};

		/// [EN] One 16-byte, 16-aligned storage cell; component copies span one or more of these so meta.copy_ writes to suitably aligned memory even as blob_ grows.
		/// [JP] 16バイト・16アラインの格納セル1つ。コンポーネントのコピーはこれを1つ以上またぎ、blob_ が伸びても meta.copy_ が適切にアラインされたメモリへ書けるようにする。
		struct alignas(16) BlobCell
		{
			Byte bytes_[16];
		};

		/// [EN] Recorded commands, replayed in this order by Flush.
		/// [JP] 記録されたコマンド。Flush がこの順で再生する。
		DynamicArray<Command> commands_;

		/// [EN] Backing storage for AddComponent component copies, referenced by Command::dataOffset_.
		/// [JP] AddComponent のコンポーネントコピーの格納領域。Command::dataOffset_ が参照する。
		DynamicArray<BlobCell> blob_;

		/// [EN] Next provisional slot index handed out by CreateEntity / SpawnPrefab.
		/// [JP] CreateEntity / SpawnPrefab が次に発行する暫定スロットインデックス。
		Uint32 provisionalCount_ = 0;

		/// [EN] Provisional slot index to the real EntityID it became in the last Flush.
		/// [JP] 暫定スロットインデックスから、直近の Flush で実体化した実際の EntityID への対応。
		FlatMap<Uint32, EntityID> resolved_;
	};

	template<typename T>
	void CommandBuffer::AddComponent(EntityID id, const T& value)
	{
		AddComponent(id, ComponentRegistry::GetComponentID<T>(), &value);
	}
}
