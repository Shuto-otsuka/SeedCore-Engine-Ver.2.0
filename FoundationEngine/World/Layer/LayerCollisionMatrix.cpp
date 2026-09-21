#include <FoundationEngine/World/Layer/LayerCollisionMatrix.h>
#include <FoundationEngine/World/Layer/LayerRegistry.h>

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
