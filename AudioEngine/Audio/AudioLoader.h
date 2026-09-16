#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <AudioEngine/Audio/Sound.h>

namespace SeedCore
{
	struct LoaderSystem;

	class SEEDCORE_API AudioLoader :public NonCopyable
	{
	public:
		AudioLoader();

		~AudioLoader();

		Handle<Sound> Load(LoaderSystem& loader, String filePath);

		Sound* Get(const Handle<Sound>& handle);

		void Clear(Handle<Sound>& handle)noexcept;

		Bool Bake(String sourcePath, String cachePath);

	private:
		StablePool<Sound> pool_;

		Bool ownsComInitialize_ = false;

		Bool mfStarted_ = false;
	};
}
