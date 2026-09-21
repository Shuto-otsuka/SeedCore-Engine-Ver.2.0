#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/ReadWrite.h>
#include <FoundationEngine/World/World.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>
#include <FoundationEngine/JobSystem/JobTaskflow.h>

namespace SeedCore
{
	/**
	* [EN]
	* Fallback: for a plain (non-Read/Write-wrapped) T, the stripped
	* type is T itself.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フォールバック: 素の（Read/Write でラップされていない）T に対しては、
	* 剥がした後の型は T 自身になる。
	*/
	template<typename T>
	struct StripAccess
	{
		/// [EN] The underlying component type, with any access-qualifier wrapper removed.
		/// [JP] アクセス修飾ラッパーを取り除いた、内部のコンポーネント型。
		using Type = T;
	};

	/**
	* [EN]
	* Specialization: strips the Read<T> wrapper down to T.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 特殊化: Read<T> ラッパーを取り除いて T にする。
	*/
	template<typename T>
	struct StripAccess<Read<T>>
	{
		/// [EN] The underlying component type, with the Read<> wrapper removed.
		/// [JP] Read<> ラッパーを取り除いた、内部のコンポーネント型。
		using Type = T;
	};

	/**
	* [EN]
	* Specialization: strips the Write<T> wrapper down to T.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 特殊化: Write<T> ラッパーを取り除いて T にする。
	*/
	template<typename T>
	struct StripAccess<Write<T>>
	{
		/// [EN] The underlying component type, with the Write<> wrapper removed.
		/// [JP] Write<> ラッパーを取り除いた、内部のコンポーネント型。
		using Type = T;
	};

	/// [EN] Convenience alias for StripAccess<T>::Type.
	/// [JP] StripAccess<T>::Type の短縮エイリアス。
	template<typename T>
	using StripAccessType = typename StripAccess<T>::Type;

	/**
	* [EN]
	* Iterates every entity in a World that has all of the component
	* types Ts..., handling archetype-chunked and sparse-set-backed
	* components transparently. Ts... may be wrapped in Read<>/Write<>
	* to document (and, via GetReadSignature/GetWriteSignature, expose
	* to a scheduler) which components are read vs written, for
	* dependency/parallelism analysis.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* World 内の、コンポーネント型 Ts... を全て持つエンティティを走査する。
	* アーキタイプ・チャンク構造化コンポーネントとスパースセットで
	* 裏付けられたコンポーネントの両方を透過的に扱う。Ts... は
	* Read<>/Write<> でラップすることで、どのコンポーネントが読み取り/
	* 書き込み対象かを明示できる（GetReadSignature/GetWriteSignature
	* 経由でスケジューラへ公開され、依存関係/並列性の解析に使われる）。
	*/
	template<typename... Ts>
	class Query
	{
	public:
		/**
		* [EN]
		* Constructs a query over world, precomputing the combined
		* archetype signature of every archetype-stored component in Ts...
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* world に対するクエリを構築し、Ts... のうちアーキタイプ格納の
		* 全コンポーネントを合わせたシグネチャをあらかじめ計算する。
		*/
		explicit Query(World& world) :world_(world)
		{
			/// [EN] Fold over Ts...: an immediately invoked lambda per type, joined by the comma operator, runs once for each requested component.
			/// [JP] Ts... の畳み込み。型ごとにその場で呼ぶラムダをカンマ演算子でつなぎ、要求された各コンポーネントについて1回ずつ実行する。
			([&]()
				{
					/// [EN] Only archetype-stored types take part in the signature; sparse-set types are checked per entity instead.
					/// [JP] シグネチャに入るのはアーキタイプ格納の型だけ。スパースセット格納の型はエンティティごとに確認する。
					if constexpr (ComponentTraits<StripAccessType<Ts>>::storage == ComponentStorage::Archetype)
					{
						/// [EN] Same bit numbering as Archetype::Signature(): the component's dense internal ID, growing the bitset as needed.
						/// [JP] Archetype::Signature() と同じビット番号(コンポーネントの密な内部 ID)を使い、足りなければビットセットを伸ばす。
						ComponentID id = ComponentRegistry::GetComponentID<StripAccessType<Ts>>();
						Size internalID = ComponentRegistry::GetID(id);
						if (internalID >= signature_.size())
						{
							signature_.resize(internalID + 1);
						}
						signature_.set(internalID);
					}
				}(), ...);
		}

		/**
		* [EN]
		* Invokes function for every entity matching this query. Skips
		* archetypes not matching signature_ entirely; for archetypes
		* that do match, resolves each archetype-stored component's base
		* pointer once per chunk, then per-entity resolves any
		* sparse-set-stored components (skipping the entity if any are
		* missing). function may optionally take EntityID as its first
		* parameter.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このクエリに一致する各エンティティに対して function を呼び出す。
		* signature_ に一致しないアーキタイプは丸ごとスキップする。
		* 一致するアーキタイプについては、チャンクごとに1回、各アーキ
		* タイプ格納コンポーネントの基底ポインタを解決し、エンティティ
		* ごとにスパースセット格納コンポーネントを解決する（いずれかが
		* 無ければそのエンティティをスキップする）。function は任意で
		* 第一引数に EntityID を受け取れる。
		*/
		template<typename F>
		void ForEach(F&& function)
		{
			/// [EN] Guards the traversal: any structural change on world_ while this is alive asserts, since it can reallocate the chunks being iterated.
			/// [JP] 走査を保護する: これが生存中に world_ へ構造変更があるとアサートする。反復中のチャンクを再確保し得るため。
			struct IterationScope
			{
				World& world_;
				explicit IterationScope(World& world) :world_(world) { world_.BeginQueryIteration(); }
				~IterationScope() { world_.EndQueryIteration(); }
				IterationScope(const IterationScope&) = delete;
				IterationScope& operator=(const IterationScope&) = delete;
			} iterationScope(world_);

			/// [EN] Walk every archetype with its chunks; the World groups chunks by the archetype they belong to.
			/// [JP] 全アーキタイプとそのチャンクを回る。World はチャンクを、属するアーキタイプごとにまとめて持っている。
			const auto& chunks = world_.GetChunk();
			for (auto& [archetype, chunkList] : chunks)
			{
				/// [EN] Superset test: the archetype matches when every bit of signature_ is also set in its signature.
				/// [JP] 包含の判定: signature_ の全ビットがアーキタイプのシグネチャにも立っていれば一致とみなす。
				const Bitset& archetypeSignature = archetype->Signature();
				if ((signature_ & archetypeSignature) != signature_)
				{
					continue;
				}

				/// [EN] Every chunk of a matching archetype holds only matching entities, so each is processed whole.
				/// [JP] 一致したアーキタイプのチャンクには一致するエンティティしか無いので、チャンク単位でまとめて処理する。
				for (Chunk* chunk : chunkList)
				{
					ProcessChunk(archetype, chunk, function);
				}
			}
		}

		/**
		* [EN]
		* Opt-in parallel variant of ForEach: dispatches one job per
		* matching chunk to executor and blocks until all finish. The
		* engine touches nothing else in the World while this runs (the
		* BeginQueryIteration guard still asserts against structural
		* changes), and the caller's function must only read/write its
		* own entity's components, must not write shared mutable state
		* without synchronization, and must route any structural change
		* through a per-worker CommandBuffer. Intended for pure per-entity
		* work (skin matrices, instance-data packing).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ForEach の opt-in な並列版: 一致するチャンクごとに1つのジョブを
		* executor へ投入し、全て終わるまでブロックする。実行中エンジンは
		* World の他の何にも触れず（BeginQueryIteration ガードは引き続き
		* 構造変更に対してアサートする）、呼び出し側の function は自分の
		* エンティティのコンポーネントのみ読み書きし、共有可変状態を同期
		* なしで書かず、構造変更はワーカーごとの CommandBuffer 経由に
		* しなければならない。純 per-entity 計算（スキン行列、インスタンス
		* データ packing）を想定。
		*/
		template<typename F>
		void ForEachParallel(JobExecutor& executor, F&& function)
		{
			/// [EN] Same structural-change guard as ForEach; it stays alive until every job has finished below.
			/// [JP] ForEach と同じ構造変更のガード。下で全ジョブが終わるまで生き続ける。
			struct IterationScope
			{
				World& world_;
				explicit IterationScope(World& world) :world_(world) { world_.BeginQueryIteration(); }
				~IterationScope() { world_.EndQueryIteration(); }
				IterationScope(const IterationScope&) = delete;
				IterationScope& operator=(const IterationScope&) = delete;
			} iterationScope(world_);

			/// [EN] One independent task per matching chunk; chunks never share memory, so they need no ordering between them.
			/// [JP] 一致するチャンクごとに独立したタスクを1つ作る。チャンク同士はメモリを共有しないので、間に順序は要らない。
			JobTaskflow taskflow;

			const auto& chunks = world_.GetChunk();
			for (auto& [archetype, chunkList] : chunks)
			{
				const Bitset& archetypeSignature = archetype->Signature();
				if ((signature_ & archetypeSignature) != signature_)
				{
					continue;
				}

				/// [EN] A plain local pointer for each task lambda to capture by value.
				/// [JP] 各タスクのラムダが値で取り込むための、普通のローカルのポインタ。
				Archetype* archetypePtr = archetype;
				for (Chunk* chunk : chunkList)
				{
					/// [EN] function is captured by reference and shared by all tasks, hence the per-entity-only rule on the caller.
					/// [JP] function は参照で取り込み全タスクで共有するので、呼び出し側には「自分のエンティティだけ触る」決まりが要る。
					taskflow.emplace([this, archetypePtr, chunk, &function]()
						{
							ProcessChunk(archetypePtr, chunk, function);
						});
				}
			}

			/// [EN] Block until every chunk is done, so the caller sees a finished pass just like ForEach.
			/// [JP] 全チャンクが終わるまで待つ。呼び出し側からは ForEach と同じく、処理が終わった状態で戻る。
			executor.Run(taskflow).wait();
		}

		/**
		* [EN]
		* Returns the combined archetype signature of every
		* archetype-stored component in Ts... (static counterpart of the
		* constructor's signature_ computation).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Ts... のうちアーキタイプ格納の全コンポーネントを合わせた
		* シグネチャを返す（コンストラクタの signature_ 計算に対応する
		* 静的版）。
		*/
		static Bitset Signature()
		{
			Bitset signature;
			([&]()
				{
					if constexpr (ComponentTraits<StripAccessType<Ts>>::storage == ComponentStorage::Archetype)
					{
						ComponentID id = ComponentRegistry::GetComponentID<StripAccessType<Ts>>();
						Size internalID = ComponentRegistry::GetID(id);
						if (internalID >= signature.size())
						{
							signature.resize(internalID + 1);
						}
						signature.set(internalID);
					}
				}(), ...);
			return signature;
		}

		/**
		* [EN]
		* Returns the archetype signature of only the components in
		* Ts... that are wrapped in Read<> (used by a scheduler to
		* detect read-after-write hazards between queries).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Ts... のうち Read<> でラップされているコンポーネントのみの
		* アーキタイプシグネチャを返す（スケジューラがクエリ間の
		* read-after-write ハザードを検出するために使う）。
		*/
		static Bitset GetReadSignature()
		{
			Bitset signature;
			([&]()
				{
					/// [EN] Unlike Signature(), sparse-set types are included too: a hazard exists whatever the storage.
					/// [JP] Signature() と違い、スパースセット格納の型も含める。競合は格納方式に関係なく起こるため。
					if constexpr (IsReadAccess<Ts>)
					{
						ComponentID id = ComponentRegistry::GetComponentID<StripAccessType<Ts>>();
						Size internalID = ComponentRegistry::GetID(id);
						if (internalID >= signature.size())
						{
							signature.resize(internalID + 1);
						}
						signature.set(internalID);
					}
				}(), ...);
			return signature;
		}

		/**
		* [EN]
		* Returns the archetype signature of only the components in
		* Ts... that are wrapped in Write<> (used by a scheduler to
		* detect write-after-write/write-after-read hazards between queries).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Ts... のうち Write<> でラップされているコンポーネントのみの
		* アーキタイプシグネチャを返す（スケジューラがクエリ間の
		* write-after-write/write-after-read ハザードを検出するために
		* 使う）。
		*/
		static Bitset GetWriteSignature()
		{
			Bitset signature;
			([&]()
				{
					/// [EN] Includes sparse-set types for the same reason as GetReadSignature().
					/// [JP] GetReadSignature() と同じ理由で、スパースセット格納の型も含める。
					if constexpr (IsWriteAccess<Ts>)
					{
						ComponentID id = ComponentRegistry::GetComponentID<StripAccessType<Ts>>();
						Size internalID = ComponentRegistry::GetID(id);
						if (internalID >= signature.size())
						{
							signature.resize(internalID + 1);
						}
						signature.set(internalID);
					}
				}(), ...);
			return signature;
		}

	private:
		/**
		* [EN]
		* Runs function over every matching entity in one chunk: resolves
		* each archetype-stored component's base pointer once, then
		* per-entity resolves any sparse-set components (skipping the
		* entity if any are missing) and invokes function. Shared by the
		* serial ForEach and the parallel ForEachParallel (one call per
		* chunk).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1つのチャンク内の一致する各エンティティに対して function を
		* 実行する: 各アーキタイプ格納コンポーネントの基底ポインタを
		* 一度解決し、エンティティごとにスパースセットコンポーネントを
		* 解決して（いずれか欠けていればそのエンティティをスキップ）
		* function を呼ぶ。直列の ForEach と並列の ForEachParallel
		* （チャンクごとに1回呼ぶ）で共有する。
		*/
		template<typename F>
		void ProcessChunk(Archetype* archetype, Chunk* chunk, F& function)
		{
			/// [EN] One ResolvedPtr per requested type, resolved once for the whole chunk (column base pointers, or sparse-set IDs).
			/// [JP] 要求された型ごとに ResolvedPtr を1つ持ち、チャンク全体で1回だけ解決する(列の先頭ポインタ、またはスパースセット用の ID)。
			std::tuple<ResolvedPtr<StripAccessType<Ts>>...> resolved;
			(ResolveOne<StripAccessType<Ts>>(archetype, chunk, std::get<ResolvedPtr<StripAccessType<Ts>>>(resolved)), ...);

			/// [EN] Row index doubles as the element index into every archetype column of this chunk.
			/// [JP] 行番号は、このチャンクの全アーキタイプ列に対する要素番号も兼ねる。
			Size count = chunk->EntityCount();
			for (Size index = 0; index < count; ++index)
			{
				EntityID entity = chunk->EntityAt(index);

				/// [EN] Sparse-set types are not guaranteed by the archetype, so each is looked up per entity; && short-circuits on the first miss.
				/// [JP] スパースセット格納の型はアーキタイプでは保証されないので、エンティティごとに引く。&& は最初に欠けた時点で打ち切る。
				Bool hasAllSparse = ([&]()
					{
						if constexpr (ComponentTraits<StripAccessType<Ts>>::storage == ComponentStorage::SparseSet)
						{
							auto& r = std::get<ResolvedPtr<StripAccessType<Ts>>>(resolved);
							r.ptr = static_cast<StripAccessType<Ts>*>(world_.GetComponent(entity, r.id));
							return r.ptr != nullptr;
						}
						else
						{
							/// [EN] Archetype-stored types are present by construction for every row of a matching chunk.
							/// [JP] アーキタイプ格納の型は、一致したチャンクの全行に必ず存在する。
							return true;
						}
					}() && ...);

				/// [EN] Missing any sparse-set component means the entity does not match the query.
				/// [JP] スパースセット格納のコンポーネントが1つでも欠けていれば、そのエンティティはクエリに一致しない。
				if (!hasAllSparse)
				{
					continue;
				}

				/// [EN] If function accepts EntityID as its first parameter, pass it through; otherwise call it the original way. This lets one path serve both call styles.
				/// [JP] function が第一引数として EntityID を受け取れるならそれを渡し、そうでなければ元の呼び方をする。これにより1つの経路で両方の呼び出し形式に対応できる。
				if constexpr (std::is_invocable_v<F&, EntityID, StripAccessType<Ts>&...>)
				{
					function(entity, FetchOne<StripAccessType<Ts>>(std::get<ResolvedPtr<StripAccessType<Ts>>>(resolved), entity, index)...);
				}
				else
				{
					function(FetchOne<StripAccessType<Ts>>(std::get<ResolvedPtr<StripAccessType<Ts>>>(resolved), entity, index)...);
				}
			}
		}

		/**
		* [EN]
		* Forward declaration selecting a storage-specific ResolvedPtr
		* specialization based on T's ComponentTraits.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T の ComponentTraits に基づいて、ストレージ別の ResolvedPtr
		* 特殊化を選択するための前方宣言。
		*/
		template<typename T, ComponentStorage Storage = ComponentTraits<T>::storage>
		struct ResolvedPtr;

		/**
		* [EN]
		* Cached resolution state for an archetype-stored component:
		* just the base pointer into the current chunk's sub-array
		* (indexed per-entity by row).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アーキタイプ格納コンポーネント用の、キャッシュされた解決状態:
		* 現在のチャンクのサブ配列への基底ポインタのみを持つ（エンティティ
		* ごとに行でインデックスされる）。
		*/
		template<typename T>
		struct ResolvedPtr<T, ComponentStorage::Archetype>
		{
			/// [EN] Base pointer into the current chunk's sub-array for T.
			/// [JP] T に対応する、現在のチャンクのサブ配列への基底ポインタ。
			Uint8* base = nullptr;
		};

		/**
		* [EN]
		* Cached resolution state for a sparse-set-stored component: T's
		* ComponentID (resolved once) and a per-entity pointer (resolved
		* fresh for each entity, since sparse-set storage isn't
		* chunk-contiguous).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スパースセット格納コンポーネント用の、キャッシュされた解決状態:
		* T の ComponentID（一度だけ解決）と、エンティティごとのポインタ
		* （スパースセットストレージはチャンク単位で連続していないため、
		* エンティティごとに新たに解決される）。
		*/
		template<typename T>
		struct ResolvedPtr<T, ComponentStorage::SparseSet>
		{
			/// [EN] T's registered ComponentID.
			/// [JP] T の登録済み ComponentID。
			ComponentID id{};

			/// [EN] Pointer to the current entity's T instance, refreshed per-entity in ForEach.
			/// [JP] 現在のエンティティの T インスタンスへのポインタ。ForEach 内でエンティティごとに更新される。
			T* ptr = nullptr;
		};

		/**
		* [EN]
		* Resolves outPtr once per chunk: for archetype storage, finds
		* T's column within chunk and caches its base pointer; for
		* sparse-set storage, just caches T's ComponentID (the actual
		* pointer is resolved per-entity in ForEach).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* チャンクごとに1回 outPtr を解決する: アーキタイプストレージの
		* 場合、chunk 内の T の列を見つけてその基底ポインタをキャッシュ
		* する。スパースセットストレージの場合は T の ComponentID のみを
		* キャッシュする（実際のポインタは ForEach 内でエンティティ
		* ごとに解決される）。
		*/
		template<typename T>
		void ResolveOne(Archetype* archetype, Chunk* chunk, ResolvedPtr<T>& outPtr)
		{
			if constexpr (ComponentTraits<T>::storage == ComponentStorage::Archetype)
			{
				/// [EN] Find T's column: its position in the archetype layout is also its sub-array index in the chunk.
				/// [JP] T の列を探す。アーキタイプのレイアウト内の位置が、そのままチャンク内のサブ配列の番号になる。
				const auto& layout = archetype->Layout();
				ComponentID id = ComponentRegistry::GetComponentID<T>();
				for (Size index = 0; index < layout.size(); ++index)
				{
					if (layout[index] == id)
					{
						outPtr.base = chunk->Data(index);
						return;
					}
				}

				/// [EN] Not found cannot happen for a matching archetype; nullptr is left as a safe value.
				/// [JP] 一致したアーキタイプでは見つからないことは起きない。念のため nullptr を残す。
				outPtr.base = nullptr;
			}
			else
			{
				/// [EN] A sparse-set type has no column to cache, only its ID for the per-entity lookup.
				/// [JP] スパースセット格納の型にはキャッシュする列が無いので、エンティティごとの検索に使う ID だけを持つ。
				outPtr.id = ComponentRegistry::GetComponentID<T>();
			}
		}

		/**
		* [EN]
		* Returns a reference to entity's T instance from resolved,
		* indexing by row for archetype storage or dereferencing the
		* per-entity pointer for sparse-set storage.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* resolved から entity の T インスタンスへの参照を返す。
		* アーキタイプストレージの場合は行でインデックスし、スパース
		* セットストレージの場合はエンティティごとのポインタを
		* 逆参照する。
		*/
		template<typename T>
		T& FetchOne(ResolvedPtr<T>& resolved, EntityID entity, Size index)
		{
			if constexpr (ComponentTraits<T>::storage == ComponentStorage::Archetype)
			{
				/// [EN] The column is a packed T array, so the row index picks the element directly.
				/// [JP] 列は T を詰めた配列なので、行番号でそのまま要素を取り出せる。
				return reinterpret_cast<T*>(resolved.base)[index];
			}
			else
			{
				/// [EN] Already looked up for this entity in ProcessChunk's sparse-set pass.
				/// [JP] ProcessChunk のスパースセットの確認で、このエンティティ分は既に引いてある。
				return *resolved.ptr;
			}
		}

	private:
		/// [EN] The World this query iterates over.
		/// [JP] このクエリが走査対象とする World。
		World& world_;

		/// [EN] Combined archetype signature of every archetype-stored component in Ts..., precomputed at construction.
		/// [JP] Ts... のうちアーキタイプ格納の全コンポーネントを合わせたシグネチャ。構築時に計算済み。
		Bitset signature_;
	};
}
