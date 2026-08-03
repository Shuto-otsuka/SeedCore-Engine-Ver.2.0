#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	inline void* ScCriMalloc(void* obj, CriUint32 size)
	{
		void* ptr;
		ptr = malloc(size);
		return ptr;
	}

	inline void ScCriFree(void* obj, void* ptr)
	{
		free(ptr);
	}
}