#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Loads/saves a single named field through a cereal archive, independently
	* of every other field passed to the same Load()/serialize() call. If the
	* name is missing or its value fails to parse, only this field is left
	* untouched (its existing/default value) instead of the exception
	* propagating out and aborting every field queued after it in the same
	* call. Safe for both Input and Output archives (writing never throws).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* cereal アーカイブ経由で名前付きフィールドを1つだけ、同じ Load()/
	* serialize() 呼び出しの他フィールドと独立に読み書きする。名前が無い、
	* または値がパースできない場合はこのフィールドだけ既存値(デフォルト値)
	* のまま残す - 例外が外へ伝播して後続フィールドを巻き添えにしない。
	* Input/Output どちらのアーカイブでも安全に使える(書き込みは例外を
	* 投げないため)。
	*/
	template<class Archive, typename T>
	void TryLoadField(Archive& archive, const Char* name, T& value)
	{
		try
		{
			archive(cereal::make_nvp(name, value));
		}
		catch (const cereal::Exception&)
		{
			/// No Code
		}
	}
}
