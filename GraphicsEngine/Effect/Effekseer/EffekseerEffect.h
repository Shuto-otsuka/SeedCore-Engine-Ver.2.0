#pragma once
#include <FoundationEngine/Prelude.h>

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
	class SEEDCORE_API EffekseerEffect :public NonCopyable
	{
		friend class EffekseerLoader;

	public:
		EffekseerEffect() = default;
		~EffekseerEffect()noexcept = default;

		EffekseerEffect(EffekseerEffect&& other)noexcept;
		EffekseerEffect& operator=(EffekseerEffect&& other)noexcept;

	public:
		[[nodiscard]] Effekseer::EffectRef& Effect();

	private:
		Effekseer::EffectRef effect_;
	};
}
