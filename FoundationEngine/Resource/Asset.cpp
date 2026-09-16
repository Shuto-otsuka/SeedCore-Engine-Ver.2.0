#include <FoundationEngine/Resource/Asset.h>

namespace SeedCore
{
	/**
	* [EN]
	* Releases whatever this manager still holds after a per-asset
	* Unload pass. Defaults to doing nothing, for managers whose Unload
	* already leaves nothing behind.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット単位の Unload パスの後に、このマネージャがまだ保持して
	* いるものを解放する。既定では何もしない（Unload の時点で何も残らない
	* マネージャ向け）。
	*/
	void Asset::Clear(const AssetContext& context)
	{
		/// No Code
	}

	/**
	* [EN]
	* Registers factory as the manager for type. Ignores a null factory,
	* and keeps the first registration if type is already taken.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* factory を type のマネージャとして登録する。null の factory は無視し、
	* type が既に登録済みであれば最初の登録を維持する。
	*/
	void AssetRegistry::Register(AssetType type, Factory factory)
	{
		if (!factory)
		{
			return;
		}

		Registry().insert({ type, factory });
	}

	/**
	* [EN]
	* Returns every registered factory, keyed by asset type.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 登録済みの全ファクトリを、アセット種別をキーとして返す。
	*/
	const FlatMap<AssetType, AssetRegistry::Factory>& AssetRegistry::GetRegistry()
	{
		return Registry();
	}

	/**
	* [EN]
	* Returns the mutable registry, held as a function-local static so
	* registration from any translation unit reaches one instance
	* regardless of static initialization order.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 可変なレジストリを返す。関数ローカル static として保持することで、
	* どの翻訳単位からの登録も、静的初期化順に関わらず単一のインスタンスへ
	* 届く。
	*/
	FlatMap<AssetType, AssetRegistry::Factory>& AssetRegistry::Registry()
	{
		static FlatMap<AssetType, Factory> registry;
		return registry;
	}
}
