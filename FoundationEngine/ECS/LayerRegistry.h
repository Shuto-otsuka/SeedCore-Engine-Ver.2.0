#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Global registry of the engine's fixed-count Actor layer slots
	* (index 0 reserved as "Default"), used for query filtering (Physics::
	* Raycast/Spherecast/Overlap's layerMask) rather than physics
	* collision response - that is still governed separately by Jolt's
	* own STATIC/KINEMATIC/DYNAMIC ObjectLayer (see JoltLayerdef.h).
	* Unlike TagRegistry (any number of tags per Actor, dynamically
	* growing), the slot count here is fixed at LayerCount (comfortably
	* within a Uint32 layerMask's bit width) and each Actor belongs to
	* exactly one slot at a time. Every slot always has a name -
	* unrenamed slots default to a placeholder "Layer N" (index 0 is
	* "Default"). Names, together with LayerCollisionMatrix's collision
	* entries, persist to a single LayerBindings.scg file via
	* Save()/Load().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンジンの固定数の Actor レイヤースロット（インデックス0は
	* "Default"として予約）を管理するグローバルレジストリ。物理の衝突応答
	* （こちらは引き続き Jolt 自身の STATIC/KINEMATIC/DYNAMIC ObjectLayer
	* が別途担う。JoltLayerdef.h 参照）ではなく、クエリの絞り込み
	* （Physics::Raycast/Spherecast/Overlap の layerMask）に使う。
	* TagRegistry（Actor 1つにつき任意個数のタグ、動的に増える）と異なり、
	* こちらはスロット数が LayerCount 固定（Uint32 の layerMask のビット幅に
	* 余裕を持って収まる）で、各 Actor は常にちょうど1つのスロットに属する。
	* 全スロットは常に名前を持ち、リネームされていないスロットは
	* プレースホルダーの"Layer N"になる（インデックス0は"Default"）。
	* 名前は LayerCollisionMatrix の衝突エントリと合わせて、
	* Save()/Load() 経由で1つの LayerBindings.scg
	* ファイルへ永続化される。
	*/
	class SEEDCORE_API LayerRegistry
	{
	public:
		/// [EN] Fixed number of layer slots, matching the bit width of a Uint32 layerMask.
		/// [JP] レイヤースロットの固定数。Uint32 の layerMask のビット幅と一致する。
		static constexpr Size LayerCount = 16;

		/// [EN] Index of the built-in "Default" layer every new Actor starts in.
		/// [JP] 新規 Actor が最初に属する、組み込みの"Default"レイヤーのインデックス。
		static constexpr Size DefaultLayer = 0;

		/// [EN] Sentinel returned by Find() when no slot is named name.
		/// [JP] name という名前のスロットが無い場合に Find() が返す番兵値。
		static constexpr Size InvalidIndex = static_cast<Size>(-1);

		/**
		* [EN]
		* Renames the layer slot at index (no-op if index is out of range).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* index のレイヤースロットの名前を変更する（index が範囲外なら
		* 何もしない）。
		*/
		static void SetName(Size index, String name);

		/**
		* [EN]
		* Returns the name of the layer slot at index (empty only if index
		* is out of range - every in-range slot always has a name).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* index のレイヤースロットの名前を返す（範囲外の場合のみ空文字列 -
		* 範囲内のスロットは常に名前を持つ）。
		*/
		static const String& GetName(Size index);

		/**
		* [EN]
		* Returns the index of the layer slot named name, or InvalidIndex
		* if no slot has that name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* name という名前のレイヤースロットのインデックスを返す。該当する
		* スロットが無ければ InvalidIndex を返す。
		*/
		static Size Find(const String& name);

		/**
		* [EN]
		* Returns whether index names a slot with a non-empty name (true
		* for every in-range slot under normal use, since unrenamed slots
		* already default to a placeholder "Layer N" name - only false if
		* index is out of range, or a slot's name was explicitly cleared
		* via SetName(index, "")).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* index が空でない名前を持つスロットを指しているかを返す（通常の
		* 使用では範囲内の全スロットで true - リネームされていないスロットも
		* 既にプレースホルダーの"Layer N"という名前を持つため。false になる
		* のは index が範囲外の場合、または SetName(index, "") で名前を
		* 明示的に空にした場合のみ）。
		*/
		static Bool IsUsed(Size index);

		/**
		* [EN]
		* Returns every layer slot's name, indexed by slot index (always
		* LayerCount entries long; every in-range slot has a name).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全レイヤースロットの名前を、スロットインデックスでアクセス
		* できる形で返す（常に LayerCount 個。範囲内のスロットは常に
		* 名前を持つ）。
		*/
		static const DynamicArray<String>& GetNames();

		/**
		* [EN]
		* Reads layer names and LayerCollisionMatrix's collision entries
		* from a single combined LayerBindings.scg (JSON) at path,
		* replacing both's current in-memory data. No-op if the file
		* doesn't exist or can't be read. Names always end up exactly
		* LayerCount entries long (padded with placeholder "Layer N"
		* names, or truncated, if the file was written under a different
		* LayerCount); LayerCollisionMatrix's entries are left at default
		* instead of being resized if their count doesn't match.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1つにまとまった LayerBindings.scg（JSON、path）から、レイヤー名と
		* LayerCollisionMatrix の衝突エントリを読み込み、両方の現在の
		* メモリ上のデータを置き換える。ファイルが存在しない/読み込めない
		* 場合は何もしない。名前は常にちょうど LayerCount 個になる（別の
		* LayerCount で書き出されたファイルの場合は、プレースホルダーの
		* "Layer N" で埋めるか切り詰める）。LayerCollisionMatrix の
		* エントリは、個数が一致しない場合はリサイズせず既定値のままにする。
		*/
		static void Load(const std::filesystem::path& path = "../UserProject/Assets/Config/LayerBindings.scg");

		/**
		* [EN]
		* Writes layer names and LayerCollisionMatrix's collision entries
		* together to path as a single LayerBindings.scg (JSON).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* レイヤー名と LayerCollisionMatrix の衝突エントリを、1つにまとめて
		* JSON（LayerBindings.scg）として path へ書き込む。
		*/
		static void Save(const std::filesystem::path& path = "../UserProject/Assets/Config/LayerBindings.scg");

	private:
		/**
		* [EN]
		* Returns the backing name array, lazily initializing it (Default-
		* Layer starts named "Default"; every other slot starts named with
		* a placeholder "Layer N") on first use.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部の名前配列を返す。初回使用時に遅延初期化する（DefaultLayer
		* だけ最初から"Default"という名前を持ち、それ以外は全てプレース
		* ホルダーの"Layer N"という名前で始まる）。
		*/
		static DynamicArray<String>& Names();

		/**
		* [EN]
		* Returns the name a slot falls back to before it's ever been
		* explicitly renamed - shared between Names()'s lazy init and
		* Load()'s padding of a short/stale array.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スロットが一度も明示的にリネームされていない時のデフォルト名を
		* 返す - Names() の遅延初期化と、Load() が短い/古い配列を埋める
		* 処理の両方で共有する。
		*/
		static String PlaceholderName(Size index);
	};
}
