#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	template<typename T>
	class DynamicArray
	{
	public:

	private:
		T* data_ = nullptr;
		Size size_ = 0;
		Size capacity_ = 0;
	};
}
