#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>

namespace SeedCore
{
	/// [EN] Wraps Effekseer::EffectRef with an explicitly noexcept move
	///      constructor/destructor so it satisfies StablePool's Poolable
	///      concept — Effekseer::RefPtr<T> has no non-template move
	///      constructor and none of its special members are marked
	///      noexcept, even though in practice they never throw (plain
	///      pointer/refcount manipulation, no allocation).
	/// [JP] Effekseer::EffectRefを、明示的にnoexceptなmoveコンストラクタ/
	///      デストラクタでラップし、StablePoolのPoolable制約を満たす。
	///      Effekseer::RefPtr<T>はテンプレートでないmoveコンストラクタを
	///      持たず、どの特殊メンバもnoexceptと宣言されていない
	///      （実際にはポインタ/参照カウント操作のみで確保もなく、
	///      投げることはない）。
	struct EffekseerEffectHandle
	{
		Effekseer::EffectRef effect_;

		EffekseerEffectHandle() = default;
		EffekseerEffectHandle(const EffekseerEffectHandle&) = default;
		EffekseerEffectHandle& operator=(const EffekseerEffectHandle&) = default;

		EffekseerEffectHandle(EffekseerEffectHandle&& other)noexcept;

		EffekseerEffectHandle& operator=(EffekseerEffectHandle&& other)noexcept;

		~EffekseerEffectHandle()noexcept = default;
	};

	class EffekseerLoader :public NonCopyable
	{
	public:
		EffekseerLoader() = default;
		~EffekseerLoader() = default;

		Handle<EffekseerEffectHandle> Load(String filePath);

		Effekseer::EffectRef* Get(const Handle<EffekseerEffectHandle>& handle);

		void Clear(Handle<EffekseerEffectHandle>& handle)noexcept;

	private:
		StablePool<EffekseerEffectHandle> pool_;
	};
}
