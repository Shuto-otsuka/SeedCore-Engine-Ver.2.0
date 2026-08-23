#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	/// [EN] The editor-facing value kind of a reflected field, driving which inspector widget is drawn for it.
	/// [JP] リフレクションされたフィールドの、エディタ向けの値種別。どのインスペクタウィジェットを描画するかを決める。
	enum class AttributeType
	{
		/// [EN] Unrecognized/unsupported type; no specialized AttributeTraits exists for it.
		/// [JP] 未認識・未対応の型。専用の AttributeTraits 特殊化が存在しない。
		Unknown,

		/// [EN] Integer value.
		/// [JP] 整数値。
		Int,

		/// [EN] Floating-point value.
		/// [JP] 浮動小数点値。
		Float,

		/// [EN] Boolean value.
		/// [JP] 真偽値。
		Bool,

		/// [EN] 2D vector value.
		/// [JP] 2次元ベクトル値。
		Vector2,

		/// [EN] 3D vector value.
		/// [JP] 3次元ベクトル値。
		Vector3,

		/// [EN] String value.
		/// [JP] 文字列値。
		String,

		/// [EN] Color value.
		/// [JP] カラー値。
		Color,

		/// [EN] Enum value (drawn as a dropdown populated from EnumRegistry).
		/// [JP] 列挙値（EnumRegistry から取得したドロップダウンとして描画される）。
		Enum,

		/// [EN] Nested reflectable struct/class (see FieldInfo::nestedTypeName_
		///      for which type). Distinct from Unknown - Unknown means "no
		///      widget exists for this", Struct means "recurse via
		///      ReflectionRegistry and draw nestedTypeName_'s own fields as a
		///      child group".
		/// [JP] ネストされたリフレクション対象の struct/class(どの型かは
		///      FieldInfo::nestedTypeName_ 参照)。Unknown とは別物 —
		///      Unknown は「対応するウィジェットが無い」、Struct は
		///      「ReflectionRegistry 経由で再帰し、nestedTypeName_ 自身の
		///      フィールドを子グループとして描画する」を意味する。
		Struct,
	};

	/**
	* [EN]
	* A single named value of a reflected enum type.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクションされた列挙型の、名前付きの単一の値。
	*/
	struct EnumEntry
	{
		/// [EN] The enum's underlying integer value.
		/// [JP] 列挙値の基底となる整数値。
		Int value_;

		/// [EN] The enum value's display name.
		/// [JP] 列挙値の表示名。
		String name_;
	};

	/// [EN] Identifies the kind of asset a payload (asset-reference) field points at, driving which asset browser/drop target the inspector shows.
	/// [JP] ペイロード（アセット参照）フィールドが指すアセットの種類を識別する。どのアセットブラウザ/ドロップターゲットをインスペクタに表示するかを決める。
	enum class PayloadAssetType
	{
		/// [EN] Not a payload field.
		/// [JP] ペイロードフィールドではない。
		None,

		/// [EN] Texture asset.
		/// [JP] テクスチャアセット。
		Texture,

		/// [EN] Model asset.
		/// [JP] モデルアセット。
		Model,

		/// [EN] Effect asset.
		/// [JP] エフェクトアセット。
		Effect,

		/// [EN] Audio asset.
		/// [JP] オーディオアセット。
		Audio,

		/// [EN] Font asset.
		/// [JP] フォントアセット。
		Font,

		/// [EN] Movie asset.
		/// [JP] ムービーアセット。
		Movie,

		/// [EN] Animation asset.
		/// [JP] アニメーションアセット。
		Animation,

		/// [EN] Baked mesh collision geometry asset (.collision).
		/// [JP] 焼き込み済みの衝突ジオメトリアセット（.collision）。
		MeshCollision,

		/// [EN] Sky asset.
		/// [JP] スカイアセット。
		Sky,

		/// [EN] Prefab asset.
		/// [JP] プレハブアセット。
		Prefab,

		/// [EN] Reference to a live Actor instance within the same World (resolved via Actor::GetPersistentID/World::FindActor), not a ResourceCache-backed asset like every other member of this enum. The underlying field is still a plain Uint32, holding the target's persistent ID (0 = unset).
		/// [JP] 同じ World 内に存在する Actor インスタンスへの参照(Actor::GetPersistentID/World::FindActor 経由で解決する)。この enum の他のメンバと異なり、ResourceCache 経由のアセットではない。実体のフィールドは通常の Uint32 で、ターゲットの永続ID を保持する(0 = 未設定)。
		Actor,
	};

	/**
	* [EN]
	* Editor bookkeeping for a reflected array-valued field: hooks to
	* grow/shrink the array and to reach the just-added last element,
	* letting the inspector manipulate the array without knowing its
	* concrete container type.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクションされた配列値フィールド用の、エディタ側の管理情報。
	* 配列を伸縮させるフックと、追加直後の末尾要素へアクセスするフックを
	* 持ち、インスペクタが具体的なコンテナ型を知らずに配列を操作できる
	* ようにする。
	*/
	struct ArrayInfo
	{
		/// [EN] Current number of elements in the array.
		/// [JP] 配列内の現在の要素数。
		Size size_ = 0;

		/// [EN] Appends a new default-constructed element to the array.
		/// [JP] 配列へ新しいデフォルト構築済みの要素を追加する。
		std::function<void()> add_;

		/// [EN] Removes the element at the given index from the array.
		/// [JP] 指定されたインデックスの要素を配列から削除する。
		std::function<void(Size)> remove_;

		/// [EN] Payload arrays only: pointer to the just-grown last element,
		///      valid right after add_() runs. Lets a drop handler grow the
		///      array and write the dropped value in one step, without the
		///      caller needing to know the container type.
		/// [JP] Payload配列専用: add_() 実行直後に有効な、追加された末尾要素への
		///      ポインタ。ドロップハンドラが配列を伸ばしてからその場で値を
		///      書き込めるようにする(呼び出し側はコンテナの型を知らなくていい)。
		std::function<void*()> lastPtr_;
	};

	/**
	* [EN]
	* Editor bookkeeping for a reflected enum-valued field: identifies
	* which enum type's entries (registered in EnumRegistry) to draw as
	* the dropdown's choices.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクションされた列挙値フィールド用の、エディタ側の管理情報。
	* ドロップダウンの選択肢として描画すべき、（EnumRegistry に登録
	* されている）列挙型の名前を識別する。
	*/
	struct EnumInfo
	{
		/// [EN] Name of the enum type this field's value belongs to, used to look up its entries in EnumRegistry.
		/// [JP] このフィールドの値が属する列挙型の名前。EnumRegistry からそのエントリ一覧を検索するために使う。
		String typeName_;
	};

	/**
	* [EN]
	* Describes a single reflected field of a component/type: its name,
	* byte offset (or direct pointer), value kind, and any editor-only
	* metadata (array/enum bookkeeping, enable-if predicate, clamp
	* range, payload asset type, visibility).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コンポーネント/型のリフレクションされた単一フィールドを記述する:
	* 名前、バイトオフセット（または直接ポインタ）、値種別、およびエディタ
	* 専用のメタデータ（配列/列挙の管理情報、有効化条件述語、クランプ
	* 範囲、ペイロードのアセット種別、表示可否）。
	*/
	struct FieldInfo
	{
		/// [EN] The field's display name.
		/// [JP] フィールドの表示名。
		String name_;

		/// [EN] Byte offset of the field within its owning type (used when directPtr_ is not set).
		/// [JP] 所有側の型内におけるフィールドのバイトオフセット（directPtr_ が設定されていない場合に使用）。
		Size offset_;

		/// [EN] The field's editor-facing value kind.
		/// [JP] フィールドの、エディタ向けの値種別。
		AttributeType type_;

		/// [EN] If this is a payload (asset-reference) field, the kind of asset it references; None otherwise.
		/// [JP] このフィールドがペイロード（アセット参照）フィールドであれば、参照するアセットの種類。そうでなければ None。
		PayloadAssetType assetType_ = PayloadAssetType::None;

		/// [EN] Direct pointer to the field's storage, used instead of offset_ when the field isn't part of a POD layout.
		/// [JP] フィールドのストレージへの直接ポインタ。フィールドが POD レイアウトの一部でない場合に offset_ の代わりに使用される。
		void* directPtr_ = nullptr;

		/// [EN] Array bookkeeping, populated when this field is array-valued.
		/// [JP] このフィールドが配列値である場合に設定される、配列の管理情報。
		ArrayInfo array_;

		/// [EN] Enum bookkeeping, populated when type_ is AttributeType::Enum.
		/// [JP] type_ が AttributeType::Enum の場合に設定される、列挙の管理情報。
		EnumInfo enum_;

		/// [EN] Predicate controlling whether the inspector should currently enable editing of this field, given the owning instance.
		/// [JP] 所有側インスタンスを受け取り、インスペクタが現在このフィールドの編集を有効にすべきかどうかを判定する述語。
		std::function<Bool(void*)> enableIf_;

		/// [EN] Minimum value the inspector should clamp this field to (numeric fields only).
		/// [JP] インスペクタがこのフィールドをクランプすべき最小値（数値フィールドのみ）。
		Float clampMin_ = -FLT_MAX;

		/// [EN] Maximum value the inspector should clamp this field to (numeric fields only).
		/// [JP] インスペクタがこのフィールドをクランプすべき最大値（数値フィールドのみ）。
		Float clampMax_ = FLT_MAX;

		/// [EN] For nested/composite fields, the display name of the nested type.
		/// [JP] ネストされた複合フィールドの場合の、ネストされた型の表示名。
		String nestedTypeName_;

		/// [EN] Whether this field should currently be shown in the inspector.
		/// [JP] このフィールドを現在インスペクタに表示すべきかどうか。
		Bool editorVisible_ = true;
	};

	/**
	* [EN]
	* Process-wide registry mapping a reflected type's name to the
	* function that enumerates its FieldInfo list, used by the editor's
	* inspector to draw any registered type generically. Populated by
	* codegen (see Tools/Python/Reflection.py) from SC_REFLECTION_FIELD*
	* annotations.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクションされた型の名前を、その FieldInfo 一覧を列挙する関数へ
	* 対応付ける、プロセス全体で共有されるレジストリ。エディタの
	* インスペクタが、登録済みの任意の型を汎用的に描画するために使う。
	* SC_REFLECTION_FIELD* アノテーションからコード生成
	* （Tools/Python/Reflection.py を参照）によって埋められる。
	*/
	class SEEDCORE_API ReflectionRegistry
	{
	public:
		/// [EN] Function type that fills fields with the FieldInfo list describing the type pointed to by instance.
		/// [JP] instance が指す型を記述する FieldInfo 一覧を fields へ書き込む、関数型。
		using ReflectFunc = std::function<void(void*, DynamicArray<FieldInfo>&)>;

		/**
		* [EN]
		* Returns the full registry mapping reflected type names to their ReflectFunc.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* リフレクションされた型名をその ReflectFunc へ対応付ける、完全な
		* レジストリを返す。
		*/
		static FlatMap<String, ReflectFunc>& GetRegistry();

		/**
		* [EN]
		* Registers function as the reflector for the type named name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* name という名前の型に対するリフレクタとして function を登録する。
		*/
		static void Register(String name, ReflectFunc function);
	};

	/**
	* [EN]
	* Process-wide registry of reflected enum types' entries, populated
	* by codegen so the editor's inspector can draw an enum-valued field
	* as a dropdown of named values.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクションされた列挙型のエントリ一覧を保持する、プロセス全体で
	* 共有されるレジストリ。コード生成によって埋められ、エディタの
	* インスペクタが列挙値フィールドを名前付きの値のドロップダウンとして
	* 描画できるようにする。
	*/
	class SEEDCORE_API EnumRegistry
	{
	public:
		/**
		* [EN]
		* Returns the full registry mapping enum type names to their entries.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 列挙型名をそのエントリ一覧へ対応付ける、完全なレジストリを返す。
		*/
		static FlatMap<String, DynamicArray<EnumEntry>>& GetRegistry();

		/**
		* [EN]
		* Registers entries as the named/valued entries of the enum type typeName.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* typeName という列挙型の名前付きエントリ一覧として entries を
		* 登録する。
		*/
		static void Register(String typeName, DynamicArray<EnumEntry> entries);

		/**
		* [EN]
		* Returns the entries registered for typeName, or nullptr if that
		* enum type is unregistered.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* typeName に対して登録されているエントリ一覧を返す。その列挙型が
		* 未登録であれば nullptr を返す。
		*/
		static const DynamicArray<EnumEntry>* GetEntries(const String& typeName);
	};

	/**
	* [EN]
	* Fallback: maps any unspecialized T to AttributeType::Unknown.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フォールバック: 特殊化されていない任意の T を AttributeType::Unknown
	* へ対応付ける。
	*/
	template<typename T>
	struct AttributeTraits
	{
		/// [EN] The editor-facing value kind for T.
		/// [JP] T に対応する、エディタ向けの値種別。
		static constexpr AttributeType type = AttributeType::Unknown;
	};

	/**
	* [EN]
	* Specialization mapping Int to AttributeType::Int.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Int を AttributeType::Int へ対応付ける特殊化。
	*/
	template<>
	struct AttributeTraits<Int>
	{
		/// [EN] The editor-facing value kind for Int.
		/// [JP] Int に対応する、エディタ向けの値種別。
		static constexpr AttributeType type = AttributeType::Int;
	};

	/**
	* [EN]
	* Specialization mapping Float to AttributeType::Float.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Float を AttributeType::Float へ対応付ける特殊化。
	*/
	template<>
	struct AttributeTraits<Float>
	{
		/// [EN] The editor-facing value kind for Float.
		/// [JP] Float に対応する、エディタ向けの値種別。
		static constexpr AttributeType type = AttributeType::Float;
	};

	/**
	* [EN]
	* Specialization mapping Bool to AttributeType::Bool.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Bool を AttributeType::Bool へ対応付ける特殊化。
	*/
	template<>
	struct AttributeTraits<Bool>
	{
		/// [EN] The editor-facing value kind for Bool.
		/// [JP] Bool に対応する、エディタ向けの値種別。
		static constexpr AttributeType type = AttributeType::Bool;
	};

	/**
	* [EN]
	* Specialization mapping Vector2 to AttributeType::Vector2.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Vector2 を AttributeType::Vector2 へ対応付ける特殊化。
	*/
	template<>
	struct AttributeTraits<Vector2>
	{
		/// [EN] The editor-facing value kind for Vector2.
		/// [JP] Vector2 に対応する、エディタ向けの値種別。
		static constexpr AttributeType type = AttributeType::Vector2;
	};

	/**
	* [EN]
	* Specialization mapping Vector3 to AttributeType::Vector3.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Vector3 を AttributeType::Vector3 へ対応付ける特殊化。
	*/
	template<>
	struct AttributeTraits<Vector3>
	{
		/// [EN] The editor-facing value kind for Vector3.
		/// [JP] Vector3 に対応する、エディタ向けの値種別。
		static constexpr AttributeType type = AttributeType::Vector3;
	};

	/**
	* [EN]
	* Specialization mapping String to AttributeType::String.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* String を AttributeType::String へ対応付ける特殊化。
	*/
	template<>
	struct AttributeTraits<String>
	{
		/// [EN] The editor-facing value kind for String.
		/// [JP] String に対応する、エディタ向けの値種別。
		static constexpr AttributeType type = AttributeType::String;
	};

	/**
	* [EN]
	* Specialization mapping Color to AttributeType::Color.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Color を AttributeType::Color へ対応付ける特殊化。
	*/
	template<>
	struct AttributeTraits<Color>
	{
		/// [EN] The editor-facing value kind for Color.
		/// [JP] Color に対応する、エディタ向けの値種別。
		static constexpr AttributeType type = AttributeType::Color;
	};
}
