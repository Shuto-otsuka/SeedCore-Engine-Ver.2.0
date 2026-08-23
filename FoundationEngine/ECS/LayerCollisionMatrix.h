#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class JsonOutputArchive;
	class JsonInputArchive;

	/**
	* [EN]
	* Global matrix (mirrors LayerRegistry's fixed LayerCount slots)
	* recording whether two Actor Layers are allowed to physically
	* collide with each other - the data backing an editor-facing Layer
	* Collision Matrix (Unity's Physics settings-style grid). Symmetric
	* by construction (only stores layerA<=layerB), and consulted by
	* ObjLayerPairFilterImplementation (JoltLayerdef.h) alongside the
	* existing STATIC/KINEMATIC/DYNAMIC motion-type rules - both must
	* allow collision for two bodies to actually collide. Every pair
	* starts enabled (collides), matching Unity's default.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つの Actor レイヤーが物理的に衝突してよいかを記録する、グローバルな
	* マトリクス（LayerRegistry の固定 LayerCount スロットと対応）。
	* エディタ向けの Layer Collision Matrix（Unity の Physics 設定画面の
	* グリッド相当）を支えるデータ。構造上対称（layerA<=layerB のみ格納）で、
	* 既存の STATIC/KINEMATIC/DYNAMIC 運動タイプルールと併せて
	* ObjLayerPairFilterImplementation（JoltLayerdef.h）から参照される
	* - 実際に衝突するには両方が許可している必要がある。全ペアは
	* Unity のデフォルトに合わせ、最初は有効（衝突する）。
	*/
	class SEEDCORE_API LayerCollisionMatrix
	{
	public:
		/**
		* [EN]
		* Returns whether layerA and layerB are currently allowed to
		* collide (true if either index is out of range).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* layerA と layerB が現在衝突を許可されているかを返す（どちらかの
		* インデックスが範囲外なら true）。
		*/
		static Bool GetCollide(Size layerA, Size layerB);

		/**
		* [EN]
		* Sets whether layerA and layerB are allowed to collide (no-op if
		* either index is out of range).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* layerA と layerB の衝突可否を設定する（どちらかのインデックスが
		* 範囲外なら何もしない）。
		*/
		static void SetCollide(Size layerA, Size layerB, Bool collide);

		/**
		* [EN]
		* Writes the current collision entries into archive under
		* "collisionEntries" - called by LayerRegistry::Save() as
		* part of the combined LayerBindings.scg write, not on its own.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の衝突エントリを "collisionEntries" として archive へ書き込む
		* - LayerBindings.scg への一括書き込みの一部として
		* LayerRegistry::Save() から呼ばれる。単独では使わない。
		*/
		static void Save(JsonOutputArchive& archive);

		/**
		* [EN]
		* Reads collision entries from archive, replacing the current
		* in-memory entries. Discarded (left at default) if the loaded
		* entry count doesn't match the current LayerRegistry::LayerCount
		* (e.g. the file was written under a different LayerCount) - called
		* by LayerRegistry::Load() as part of the combined
		* LayerBindings.scg read, not on its own.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* archive から衝突エントリを読み込み、現在のメモリ上のエントリを
		* 置き換える。読み込んだエントリ数が現在の
		* LayerRegistry::LayerCount と一致しない場合(別の LayerCount で
		* 書き出されたファイルなど)は破棄する(既定値のまま) -
		* LayerBindings.scg の一括読み込みの一部として
		* LayerRegistry::Load() から呼ばれる。単独では使わない。
		*/
		static void Load(JsonInputArchive& archive);

	private:
		/**
		* [EN]
		* Returns the backing triangular entry array, lazily
		* initializing it (every pair enabled) on first use.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部の三角配列を返す。初回使用時に遅延初期化する（全ペア有効）。
		*/
		static DynamicArray<Bool>& Entries();

		/**
		* [EN]
		* Maps an unordered (layerA, layerB) pair to its index within
		* the triangular Entries() array.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 順不同の (layerA, layerB) の組を、三角配列 Entries() 内の
		* インデックスへ対応付ける。
		*/
		static Size TriangleIndex(Size layerA, Size layerB);
	};
}
