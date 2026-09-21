#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Allocation callback handed to CRI Atom and CRI File System, so their
	* internal work memory comes from the process heap. It is only ever
	* called through a function pointer from inside the CRI libraries.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CRI Atom と CRI File System に渡す確保コールバック。両者の内部の作業
	* メモリをプロセスのヒープから確保させる。CRI ライブラリの内部から、
	* 関数ポインタ経由でのみ呼ばれる。
	*/
	inline void* ScCriMalloc(void* obj, CriUint32 size)
	{
		/// [EN] obj is the user object registered with the callback; none is registered, so it is unused.
		/// [JP] obj はコールバックと一緒に登録したユーザーオブジェクト。何も登録していないので使わない。
		void* ptr;
		ptr = malloc(size);
		return ptr;
	}

	/**
	* [EN]
	* Release callback paired with ScCriMalloc; returns memory CRI no
	* longer needs to the process heap.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ScCriMalloc と対になる解放コールバック。CRI が不要になったメモリを
	* プロセスのヒープへ返す。
	*/
	inline void ScCriFree(void* obj, void* ptr)
	{
		free(ptr);
	}
}
