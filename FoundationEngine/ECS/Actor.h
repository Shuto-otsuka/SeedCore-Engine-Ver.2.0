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
	* Lightweight value handle to a GameObject-style actor: pairs the
	* owning World with a generation-checked Entity, and forwards every
	* operation (hierarchy, active state, tags, layers, components,
	* prefab bookkeeping) to World, where the actor's data actually
	* lives. Copy it freely; it is 16 bytes and never dangles - a call
	* on an actor whose entity has been destroyed resolves to nothing
	* (nullptr / default) rather than aliasing a recycled slot. A
	* default-constructed Actor is invalid (converts to false).
	*
	* Individual non-template members carry SEEDCORE_API rather than the
	* class itself, because a class-level dllexport/dllimport propagates
	* to member function templates under Clang (unlike MSVC).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GameObject 風の actor への軽量な値ハンドル: 所有元 World と世代
	* チェック付き Entity のペアで、全操作（階層、アクティブ状態、タグ、
	* レイヤー、コンポーネント、プレハブ管理）を、actor のデータが実際に
	* 存在する World へ転送する。自由にコピーしてよい。16 バイトで、
	* ダングリングしない - エンティティが破棄された actor への呼び出しは、
	* 再利用スロットにエイリアスするのではなく、何も無い結果
	* （nullptr / デフォルト）に解決される。デフォルト構築された Actor は
	* 無効（false に変換される）。
	*/
	class Actor
	{
	public:
		/**
		* [EN]
		* Default constructor: an invalid actor (no World, null entity).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 無効な actor（World 無し、null エンティティ）。
		*/
		Actor() = default;

		/**
		* [EN]
		* Constructs a handle to the actor identified by entity within
		* world. Does not create anything - use World::CreateActor for that.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* world 内で entity が識別する actor へのハンドルを構築する。何も
		* 生成しない - 生成には World::CreateActor を使う。
		*/
		SEEDCORE_API Actor(World& world, Entity entity);

		/**
		* [EN]
		* Returns whether this handle refers to a live actor, letting an
		* Actor be used directly in a boolean context (if, &&, ?:).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このハンドルが生存中の actor を指しているかどうかを返す。Actor を
		* 真偽値コンテキスト（if、&&、?:）でそのまま使えるようにする。
		*/
		SEEDCORE_API explicit operator Bool()const;

		/**
		* [EN]
		* Adds a copy of value as this actor's T component (T not deriving
		* from ComponentBase).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value のコピーをこの actor の T コンポーネント（T は ComponentBase
		* から派生しない）として追加する。
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
		* Type-erased overload of AddComponent: adds a default-constructed
		* instance of the component registered under id, wiring up its
		* lifecycle if it derives from ComponentBase.
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
		* Returns a mutable pointer to this actor's T component (T deriving
		* from ComponentBase), or nullptr if absent.
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
		* Removes this actor's component registered under id, invoking its
		* OnDestroy first if it derives from ComponentBase.
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
		* この actor に現在アタッチされている、全ての ComponentBase 派生
		* コンポーネントの ID 一覧を返す。
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
		* Reparents this actor under parent (or to the scene root if parent
		* is an invalid Actor), updating both the old and new parent's
		* children lists, and inheriting the new parent's active state.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor を parent の下へ付け替える（parent が無効な Actor で
		* あればシーンのルートへ）。旧・新それぞれの親の子リストを更新し、
		* 新しい親のアクティブ状態を継承する。
		*/
		SEEDCORE_API void SetParent(Actor parent);

		/**
		* [EN]
		* Returns this actor's current parent, or an invalid Actor if it
		* has none.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の現在の親を返す。親が無ければ無効な Actor を返す。
		*/
		SEEDCORE_API Actor GetParent()const;

		/**
		* [EN]
		* Returns this actor's direct children, in their current order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の直接の子一覧を、現在の順序で返す。
		*/
		SEEDCORE_API DynamicArray<Actor> GetChildren()const;

		/**
		* [EN]
		* Repositions child (must already be a child of this Actor) so it
		* comes immediately after after in the children order. An invalid
		* after moves child to the front; an after that isn't among the
		* children appends it to the end.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* child（既にこの Actor の子であること）を、子の並び順で after の
		* 直後に来るよう再配置する。after が無効なら先頭へ、after が子の
		* 中に見つからない場合は末尾に移動する。
		*/
		SEEDCORE_API void MoveChild(Actor child, Actor after);

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
		SEEDCORE_API Bool Descendant(Actor ancestor)const;

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
		SEEDCORE_API Bool GetActive()const;

		/**
		* [EN]
		* Sets this actor's active state (updating both the cached flag and
		* its Active component if present), and propagates the same state
		* to every descendant.
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
		* Adds tag to this actor's tag set, registering it in TagRegistry
		* if it's new.
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
		* Sets this actor's layer to the LayerRegistry slot at index
		* (clamped to a valid slot if out of range).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor のレイヤーを、index の LayerRegistry スロットに設定
		* する（範囲外なら有効なスロットへクランプする）。
		*/
		SEEDCORE_API void SetLayer(Size index);

		/**
		* [EN]
		* Sets this actor's layer by name via LayerRegistry::Find, falling
		* back to LayerRegistry::DefaultLayer if name isn't registered.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* LayerRegistry::Find 経由で、名前でこの actor のレイヤーを設定
		* する。name が未登録であれば LayerRegistry::DefaultLayer に
		* フォールバックする。
		*/
		SEEDCORE_API void SetLayer(const String& name);

		/**
		* [EN]
		* Returns this actor's current layer index (a LayerRegistry slot).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の現在のレイヤーインデックス（LayerRegistry の
		* スロット）を返す。
		*/
		SEEDCORE_API Size GetLayer()const;

		/**
		* [EN]
		* Returns this actor's current layer's name, via LayerRegistry::GetName.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* LayerRegistry::GetName 経由で、この actor の現在のレイヤー名を
		* 返す。
		*/
		SEEDCORE_API const String& GetLayerName()const;

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
		SEEDCORE_API Bool GetPrefabInstance()const;

		/**
		* [EN]
		* Sets the asset ID of the source prefab this actor's instance was
		* created from (root actor only).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor のインスタンスが生成された元のプレハブの、アセット ID
		* を設定する（ルート actor のみ）。
		*/
		SEEDCORE_API void SetSourcePrefabAssetID(Uint32 assetID);

		/**
		* [EN]
		* Returns the asset ID of the source prefab this actor's instance
		* was created from, or 0 if it's not an instance root.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor のインスタンスが生成された元のプレハブの、アセット ID
		* を返す。インスタンスルートでなければ 0 を返す。
		*/
		SEEDCORE_API Uint32 GetSourcePrefabAssetID()const;

		/**
		* [EN]
		* Sets this actor's persistent ID. Only World::CreateActor should
		* call this.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の永続IDを設定する。World::CreateActor のみがこれを
		* 呼ぶべき。
		*/
		SEEDCORE_API void SetPersistentID(Uint32 id);

		/**
		* [EN]
		* Returns this actor's persistent ID: a World-wide unique
		* identifier that round-trips through Scene/Prefab serialization.
		* 0 means unassigned.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この actor の永続IDを返す: World 全体で一意な識別子であり、
		* Scene/Prefab のシリアライズを往復する。0 は未割り当てを意味する。
		*/
		SEEDCORE_API Uint32 GetPersistentID()const;

		/**
		* [EN]
		* Returns whether this and other are handles to the same actor:
		* they refer to the same World and the same Entity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* this と other が同じ actor へのハンドルかどうかを返す: 同じ
		* World と同じ Entity を指しているか。
		*/
		SEEDCORE_API Bool operator==(const Actor& other)const;

	private:
		/// [EN] The World that owns this actor's data; nullptr for a default-constructed (invalid) Actor.
		/// [JP] この actor のデータを所有する World。デフォルト構築された（無効な）Actor では nullptr。
		World* world_ = nullptr;

		/// [EN] This actor's underlying entity handle.
		/// [JP] この actor の内部エンティティハンドル。
		Entity entity_;
	};
}
