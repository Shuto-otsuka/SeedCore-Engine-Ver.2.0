#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/ECS/Component/ComponentBase.h>
#include <FoundationEngine/Utility/Bitset.h>

namespace SeedCore
{
	class World;
	class Physics;

	/**
	* [EN]
	* Higher-level, GameObject-style wrapper around a raw ECS Entity:
	* adds a scene-graph parent/child hierarchy, an active flag, tag
	* membership, prefab-instance bookkeeping, and convenience
	* component accessors on top of World's low-level entity/component API.
	*
	* Individual non-template members carry SEEDCORE_API rather than the
	* class itself, because a class-level dllexport/dllimport propagates
	* to member function templates under Clang (unlike MSVC) — any T used
	* with AddComponent<T>/GetComponent<T> from outside this DLL would
	* otherwise need its own explicit template instantiation to link under
	* ClangCL. Left as plain templates, they're instantiated locally in
	* every including translation unit instead, with no export needed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 生の ECS Entity に対する、より高水準な GameObject 風のラッパー。
	* World の低水準なエンティティ/コンポーネント API の上に、シーン
	* グラフの親子階層、アクティブフラグ、タグ所属、プレハブ
	* インスタンスの管理情報、および便利なコンポーネントアクセサを
	* 追加する。
	*
	* クラス全体にではなく、テンプレートでない個々のメンバに SEEDCORE_API
	* を付けている。クラス単位の dllexport/dllimport は、MSVCと異なり
	* Clang ではメンバ関数テンプレートにも伝播してしまうため、このDLLの
	* 外から AddComponent<T>/GetComponent<T> を使う型 T が増えるたびに、
	* ClangCLでリンクするための明示的テンプレートインスタンス化が個別に
	* 必要になってしまう。ただのテンプレートのままにしておけば、
	* #include した各翻訳単位でローカルにインスタンス化されるだけで済み、
	* エクスポートが一切不要になる。
	*/
	class Actor
	{
	public:
		/**
		* [EN]
		* Constructs an actor in world with the given display name,
		* creating its underlying entity, physics resource, and the
		* default transform/lifecycle components (Name/Position/
		* Rotation/Scale/Velocity/Active).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定された表示名で world 内に actor を構築し、内部エンティティ、
		* 物理リソース、およびデフォルトのトランスフォーム/ライフサイクル
		* コンポーネント（Name/Position/Rotation/Scale/Velocity/Active）を
		* 生成する。
		*/
		SEEDCORE_API Actor(World& world, String name = String("Actor"));

		/**
		* [EN]
		* Copy-constructs an actor (references bind to the same World/
		* Physics as source; declared explicitly, rather than left
		* compiler-generated, so it gets a stable exported symbol now
		* that the class itself is no longer whole-class exported).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actor をコピー構築する（参照メンバは source と同じ World/Physics
		* を指す）。クラス全体のエクスポートをやめたことで、暗黙生成に
		* 任せず明示的に宣言し、安定したエクスポートシンボルを持たせている。
		*/
		SEEDCORE_API Actor(const Actor&) = default;

		/**
		* [EN]
		* Move-constructs an actor. Declared explicitly for the same
		* reason as the copy constructor above.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actor をムーブ構築する。上記のコピーコンストラクタと同じ理由で
		* 明示的に宣言している。
		*/
		SEEDCORE_API Actor(Actor&&) = default;

		/**
		* [EN]
		* Destroys this actor. Declared explicitly for the same reason
		* as the copy constructor above.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor を破棄する。上記のコピーコンストラクタと同じ理由で
		* 明示的に宣言している。
		*/
		SEEDCORE_API ~Actor() = default;

		/**
		* [EN]
		* Adds a copy of value as this actor's T component (T not
		* deriving from ComponentBase).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value のコピーをこの actor の T コンポーネント（T は
		* ComponentBase から派生しない）として追加する。
		*/
		template<typename T>
			requires(!std::derived_from<T, ComponentBase>)
		void AddComponent(const T& value);

		/**
		* [EN]
		* Default-constructs and adds a T component (T deriving from
		* ComponentBase) to this actor, wiring up its lifecycle function
		* pointers. Returns a pointer to the new component.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T コンポーネント（T は ComponentBase から派生する）をデフォルト
		* 構築してこの actor へ追加し、そのライフサイクル関数ポインタを
		* 配線する。新しいコンポーネントへのポインタを返す。
		*/
		template<typename T>
			requires std::derived_from<T, ComponentBase>
		T* AddComponent();

		/**
		* [EN]
		* Type-erased overload of AddComponent: adds a
		* default-constructed instance of the component registered
		* under id, wiring up its lifecycle if it derives from ComponentBase.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* AddComponent の型消去版: id に登録されているコンポーネントの
		* デフォルト構築されたインスタンスを追加する。ComponentBase から
		* 派生していれば、そのライフサイクルを配線する。
		*/
		SEEDCORE_API void AddComponent(ComponentID id);

		/**
		* [EN]
		* Returns a const pointer to this actor's T component (T not
		* deriving from ComponentBase), or nullptr if absent.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の T コンポーネント（T は ComponentBase から派生
		* しない）への const ポインタを返す。無ければ nullptr を返す。
		*/
		template<typename T>
			requires(!std::derived_from<T, ComponentBase>)
		const T* GetComponent()const;

		/**
		* [EN]
		* Returns a mutable pointer to this actor's T component (T
		* deriving from ComponentBase), or nullptr if absent.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の T コンポーネント（T は ComponentBase から派生
		* する）への変更可能なポインタを返す。無ければ nullptr を返す。
		*/
		template<typename T>
			requires std::derived_from<T, ComponentBase>
		T* GetComponent()const;

		/**
		* [EN]
		* Removes this actor's component registered under id, invoking
		* its OnDestroy first if it derives from ComponentBase.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の、id に登録されているコンポーネントを削除する。
		* ComponentBase から派生していれば、先に OnDestroy を呼び出す。
		*/
		SEEDCORE_API void RemoveComponent(ComponentID id);

		/**
		* [EN]
		* Returns whether this actor currently has the component
		* registered under id.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor が現在、id に登録されているコンポーネントを持って
		* いるかどうかを返す。
		*/
		SEEDCORE_API Bool HasComponent(ComponentID id)const;

		/**
		* [EN]
		* Returns the IDs of every ComponentBase-derived component
		* currently attached to this actor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor に現在アタッチされている、全ての ComponentBase
		* 派生コンポーネントの ID 一覧を返す。
		*/
		SEEDCORE_API const DynamicArray<ComponentID>& ComponentBaseIDList()const;

		/**
		* [EN]
		* Returns this actor's underlying entity handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の内部エンティティハンドルを返す。
		*/
		SEEDCORE_API Entity GetEntity()const;

		/**
		* [EN]
		* Returns the World that owns this actor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor を所有する World を返す。
		*/
		SEEDCORE_API World& GetWorld()const;

		/**
		* [EN]
		* Returns this actor's Physics resource.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の Physics リソースを返す。
		*/
		SEEDCORE_API Physics& GetPhysics()const;

		/**
		* [EN]
		* Reparents this actor under parent (or to the scene root if
		* nullptr), updating both the old and new parent's children
		* lists, and inheriting the new parent's active state.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor を parent の下へ付け替える（nullptr であればシーンの
		* ルートへ）。旧・新それぞれの親の子リストを更新し、新しい親の
		* アクティブ状態を継承する。
		*/
		SEEDCORE_API void SetParent(Actor* parent);

		/**
		* [EN]
		* Returns this actor's current parent, or nullptr if it has none.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の現在の親を返す。親が無ければ nullptr を返す。
		*/
		SEEDCORE_API Actor* GetParent()const;

		/**
		* [EN]
		* Returns this actor's direct children, in their current order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の直接の子一覧を、現在の順序で返す。
		*/
		SEEDCORE_API const DynamicArray<Actor*>& GetChildren()const;

		/**
		* [EN]
		* Repositions child (must already be a child of this Actor) so it comes
		* immediately after after in the children order. Appends to the end if
		* after is not found among the children.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* child（既にこの Actor の子であること）を、子の並び順で after の
		* 直後に来るよう再配置する。after が子の中に見つからない場合は末尾に追加する。
		*/
		SEEDCORE_API void MoveChildAfter(Actor* child, Actor* after);

		/**
		* [EN]
		* Returns whether ancestor is somewhere in this actor's parent chain.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ancestor がこの actor の親チェーンのどこかに存在するかどうかを
		* 返す。
		*/
		SEEDCORE_API Bool Descendant(const Actor* ancestor)const;

		/**
		* [EN]
		* Returns this actor's cached world-space transform matrix.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の、キャッシュされたワールド空間変換行列を返す。
		*/
		SEEDCORE_API const Matrix& GetWorldMatrix()const;

		/**
		* [EN]
		* Sets this actor's cached world-space transform matrix.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の、キャッシュされたワールド空間変換行列を設定する。
		*/
		SEEDCORE_API void SetWorldMatrix(const Matrix& matrix);

		/**
		* [EN]
		* Returns whether this actor is currently active (preferring its
		* Active component's value if present, otherwise the cached flag).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor が現在アクティブかどうかを返す（Active コンポーネント
		* が存在すればその値を優先し、無ければキャッシュされたフラグを
		* 使う）。
		*/
		SEEDCORE_API Bool IsActive()const;

		/**
		* [EN]
		* Sets this actor's active state (updating both the cached flag
		* and its Active component if present), and propagates the same
		* state to every descendant.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor のアクティブ状態を設定する（キャッシュされたフラグと、
		* 存在すれば Active コンポーネントの両方を更新する）。同じ状態を
		* 全ての子孫へ伝播する。
		*/
		SEEDCORE_API void SetActive(Bool active);

		/**
		* [EN]
		* Adds tag to this actor's tag set, registering it in
		* TagRegistry if it's new.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* tag をこの actor のタグ集合へ追加する。新規のタグであれば
		* TagRegistry へ登録する。
		*/
		SEEDCORE_API void AddTag(String tag);

		/**
		* [EN]
		* Removes tag from this actor's tag set, if present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* tag をこの actor のタグ集合から、存在すれば削除する。
		*/
		SEEDCORE_API void RemoveTag(String tag);

		/**
		* [EN]
		* Returns whether this actor currently has tag.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor が現在 tag を持っているかどうかを返す。
		*/
		SEEDCORE_API Bool HasTag(String tag)const;

		/**
		* [EN]
		* Returns the names of every tag currently set on this actor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor に現在設定されている全タグの名前一覧を返す。
		*/
		SEEDCORE_API DynamicArray<String> GetTagList()const;

		/**
		* [EN]
		* Marks whether this actor was instantiated from a prefab.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor がプレハブからインスタンス化されたかどうかを
		* マークする。
		*/
		SEEDCORE_API void SetPrefabInstance(Bool value);

		/**
		* [EN]
		* Returns whether this actor was instantiated from a prefab.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor がプレハブからインスタンス化されたかどうかを返す。
		*/
		SEEDCORE_API Bool IsPrefabInstance()const;

		/**
		* [EN]
		* Sets the asset ID of the source prefab this actor's instance
		* was created from (root actor only).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor のインスタンスが生成された元のプレハブの、アセット
		* ID を設定する（ルート actor のみ）。
		*/
		SEEDCORE_API void SetSourcePrefabAssetID(Uint32 assetID);

		/**
		* [EN]
		* Returns the asset ID of the source prefab this actor's
		* instance was created from, or 0 if it's not an instance root.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor のインスタンスが生成された元のプレハブの、アセット
		* ID を返す。インスタンスルートでなければ 0 を返す。
		*/
		SEEDCORE_API Uint32 GetSourcePrefabAssetID()const;

		/**
		* [EN]
		* Sets this actor's persistent ID. Only World::CreateActor should
		* call this (it resolves collisions and keeps its ID counter in
		* sync); everyone else should treat GetPersistentID() as read-only.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の永続IDを設定する。衝突解決とIDカウンタの同期を
		* 行う World::CreateActor のみがこれを呼ぶべきであり、それ以外は
		* GetPersistentID() を読み取り専用として扱うこと。
		*/
		SEEDCORE_API void SetPersistentID(Uint32 id);

		/**
		* [EN]
		* Returns this actor's persistent ID: a World-wide unique
		* identifier that (unlike Entity, which is a runtime-only handle
		* reset on every load) round-trips through Scene/Prefab
		* serialization, so other actors (e.g. constraint targets) can
		* reference this actor stably across save/load. 0 means unassigned.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の永続IDを返す: World全体で一意な識別子であり、
		* （ロードの度にリセットされる実行時ハンドルである Entity とは
		* 異なり）Scene/Prefab のシリアライズを往復する。これにより他の
		* actor（例: コンストレイントのターゲット）が、セーブ/ロードを
		* またいでこの actor を安定して参照できる。0 は未割り当てを意味する。
		*/
		SEEDCORE_API Uint32 GetPersistentID()const;

	private:
		/// [EN] The World that owns this actor.
		/// [JP] この actor を所有する World。
		World& world_;

		/// [EN] This actor's Physics resource.
		/// [JP] この actor の Physics リソース。
		Physics& physics_;

		/// [EN] This actor's underlying entity handle.
		/// [JP] この actor の内部エンティティハンドル。
		Entity entity_;

		/// [EN] IDs of every ComponentBase-derived component currently attached to this actor.
		/// [JP] この actor に現在アタッチされている、全ての ComponentBase 派生コンポーネントの ID 一覧。
		DynamicArray<ComponentID> componentBaseIDs_;

		/// [EN] This actor's current parent, or nullptr if it has none.
		/// [JP] この actor の現在の親。親が無ければ nullptr。
		Actor* parent_ = nullptr;

		/// [EN] This actor's direct children, in display order.
		/// [JP] この actor の直接の子一覧。表示順で保持される。
		DynamicArray<Actor*> children_;

		/// [EN] Cached world-space transform matrix.
		/// [JP] キャッシュされたワールド空間変換行列。
		Matrix worldMatrix_ = Matrix::Identity;

		/// [EN] Cached active flag, used when this actor has no Active component.
		/// [JP] キャッシュされたアクティブフラグ。この actor が Active コンポーネントを持たない場合に使われる。
		Bool active_ = true;

		/// [EN] Bitset of tag membership, indexed by TagRegistry bit index.
		/// [JP] タグ所属を表すビットセット。TagRegistry のビットインデックスでアクセスする。
		Bitset tags_;

		/// [EN] Whether this actor was instantiated from a prefab.
		/// [JP] この actor がプレハブからインスタンス化されたかどうか。
		Bool fromPrefab_ = false;

		/// [EN] Only set on the root Actor of a Prefab instance; identifies which
		///      .prefab asset "Prefab に適用" should overwrite. 0 = not an instance root.
		/// [JP] Prefab インスタンスのルート Actor にのみ設定される。「Prefab に適用」が
		///      上書きすべき .prefab アセットを識別する。0 はインスタンスルートでないことを示す。
		Uint32 sourcePrefabAssetID_ = 0;

		/// [EN] World-wide unique identifier assigned by World::CreateActor, used to reference this actor stably across Scene/Prefab save/load. 0 = unassigned.
		/// [JP] World::CreateActor によって割り当てられる、World 全体で一意な識別子。Scene/Prefab のセーブ/ロードをまたいでこの actor を安定して参照するために使う。0 は未割り当て。
		Uint32 persistentId_ = 0;
	};
}
