#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerEffect.h>

namespace SeedCore
{
	class EffekseerLoader :public NonCopyable
	{
	public:
		EffekseerLoader() = default;
		~EffekseerLoader() = default;

		Handle<EffekseerEffect> Load(String filePath);

		EffekseerEffect* Get(const Handle<EffekseerEffect>& handle);

		void Clear(Handle<EffekseerEffect>& handle)noexcept;

	private:
		StablePool<EffekseerEffect> pool_;
	};
}
