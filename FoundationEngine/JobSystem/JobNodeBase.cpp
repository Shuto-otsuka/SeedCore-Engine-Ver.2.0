#include <FoundationEngine/JobSystem/JobNodeBase.h>

namespace SeedCore
{
	/**
	* [EN]
	* Explicit constructor for initializing all fields at once.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全フィールドを一括初期化するための明示的コンストラクタ。
	*/
	JobNodeBase::JobNodeBase(NState nstate, EState estate, JobNodeBase* parent, Size joinCounter) :nstate_(nstate), estate_(estate), parent_(parent), joinCounter_(joinCounter)
	{
		/// No Code
	}
}