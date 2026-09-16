#include <GraphicsEngine/Effect/Effekseer/EffekseerEffect.h>

namespace SeedCore
{
	EffekseerEffect::EffekseerEffect(EffekseerEffect&& other)noexcept :effect_(std::move(other.effect_))
	{
		/// No Code
	}

	EffekseerEffect& EffekseerEffect::operator=(EffekseerEffect&& other)noexcept
	{
		effect_ = std::move(other.effect_);
		return *this;
	}

	Effekseer::EffectRef& EffekseerEffect::Effect()
	{
		return effect_;
	}
}
