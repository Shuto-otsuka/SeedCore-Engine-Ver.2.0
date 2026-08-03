#include <FoundationEngine/ECS/EntityRecord.h>

namespace SeedCore
{
	/**
	* [EN]
	* Returns whether this record points at a live entity (all fields populated).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このレコードが有効なエンティティを指しているか（全フィールドが
	* 設定済みか）を返す。
	*/
	Bool EntityRecord::Exists()const
	{
		return archetype_ != nullptr && chunk_ != nullptr && row_ != UINT32_MAX;
	}
}
