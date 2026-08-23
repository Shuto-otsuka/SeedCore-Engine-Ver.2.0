#include <FoundationEngine/ECS/LayerCollisionMatrix.h>
#include <FoundationEngine/ECS/LayerRegistry.h>
#include <FoundationEngine/Serialization/Json/JsonArchive.h>

namespace SeedCore
{
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
	Bool LayerCollisionMatrix::GetCollide(Size layerA, Size layerB)
	{
		if (layerA >= LayerRegistry::LayerCount || layerB >= LayerRegistry::LayerCount)
		{
			return true;
		}

		return Entries()[TriangleIndex(layerA, layerB)];
	}

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
	void LayerCollisionMatrix::SetCollide(Size layerA, Size layerB, Bool collide)
	{
		if (layerA >= LayerRegistry::LayerCount || layerB >= LayerRegistry::LayerCount)
		{
			return;
		}

		Entries()[TriangleIndex(layerA, layerB)] = collide;
	}

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
	void LayerCollisionMatrix::Save(JsonOutputArchive& archive)
	{
		archive.Field("collisionEntries", Entries());
	}

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
	void LayerCollisionMatrix::Load(JsonInputArchive& archive)
	{
		DynamicArray<Bool> loaded;
		if (!archive.TryField("collisionEntries", loaded))
		{
			return;
		}

		Size expectedCount = LayerRegistry::LayerCount * (LayerRegistry::LayerCount + 1) / 2;
		if (loaded.size() != expectedCount)
		{
			return;
		}

		Entries() = std::move(loaded);
	}

	/**
	* [EN]
	* Returns the backing triangular entry array, lazily initializing
	* it (every pair enabled) on first use.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部の三角配列を返す。初回使用時に遅延初期化する（全ペア有効）。
	*/
	DynamicArray<Bool>& LayerCollisionMatrix::Entries()
	{
		static DynamicArray<Bool> entries(LayerRegistry::LayerCount * (LayerRegistry::LayerCount + 1) / 2, true);
		return entries;
	}

	/**
	* [EN]
	* Maps an unordered (layerA, layerB) pair to its index within the
	* triangular Entries() array.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 順不同の (layerA, layerB) の組を、三角配列 Entries() 内の
	* インデックスへ対応付ける。
	*/
	Size LayerCollisionMatrix::TriangleIndex(Size layerA, Size layerB)
	{
		Size lo = Min(layerA, layerB);
		Size hi = Max(layerA, layerB);
		return hi * (hi + 1) / 2 + lo;
	}
}
