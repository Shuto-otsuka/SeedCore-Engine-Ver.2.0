#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	template<typename T, Size N>
	class StaticArray
	{
	public:

	private:
		T data_[N];
	};
}
