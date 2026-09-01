#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	class Archetype;

	/**
	* [EN]
	* Process-wide, static cache of Archetype instances keyed by a hash
	* of their (sorted) component layout, so that any two identical
	* layouts always resolve to the same Archetype instance rather than
	* being duplicated.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コンポーネントレイアウト（ソート済み）のハッシュをキーとした、
	* プロセス全体で共有される静的な Archetype インスタンスキャッシュ。
	* 同一のレイアウトが2つあった場合、重複して生成されるのではなく、
	* 常に同じ Archetype インスタンスへ解決される。
	*/
	class SEEDCORE_API ArchetypeRegistry
	{
	private:
		/**
		* [EN]
		* RAII helper whose sole purpose is freeing every registered
		* Archetype at static-destruction time.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 静的破棄時に登録済みの全 Archetype を解放することだけを目的とした
		* RAII ヘルパー。
		*/
		struct RegistryCleaner
		{
			/**
			* [EN]
			* Deletes every Archetype currently held in registry_.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* registry_ に現在保持されている全ての Archetype を削除する。
			*/
			~RegistryCleaner();
		};

	public:
		/**
		* [EN]
		* Returns the existing Archetype matching layout (order-independent),
		* or creates and registers a new one if none exists yet.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* layout に一致する（順序に依存しない）既存の Archetype を返す。
		* まだ存在しなければ新しく生成して登録する。
		*/
		static Archetype* GetOrCreate(const DynamicArray<ComponentID>& layout);

	private:
		/// [EN] Maps a layout hash to its Archetype instance.
		/// [JP] レイアウトハッシュを、その Archetype インスタンスへ対応付ける。
		static inline FlatMap<Size, Archetype*> registry_;

		/// [EN] Static instance whose destructor frees every registered Archetype at program exit.
		/// [JP] プログラム終了時にデストラクタが登録済みの全 Archetype を解放する、静的インスタンス。
		static inline RegistryCleaner cleaner_;
	};
}
