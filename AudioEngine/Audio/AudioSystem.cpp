#include <AudioEngine/Audio/AudioSystem.h>
#include <AudioEngine/Audio/AudioSource.h>
#include <AudioEngine/Audio/AudioListener.h>
#include <AudioEngine/Audio/AudioResource.h>
#include <AudioEngine/Audio/Sound.h>
#include <FoundationEngine/World/World.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	/**
	* [EN]
	* Assigns loaded Sound objects to sources whose asset ID has changed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID が変わった音源へ、読み込み済み Sound を割り当てる。
	*/
	void AudioSystem::ResolveSound(LoaderSystem& loader, ResourceCache& cache, World& world)
	{
		AudioResource* audioResource = cache.GetResource<AudioResource>(AssetType::Audio);
		if (!audioResource)
		{
			return;
		}

		/// [EN] Resolve only sources whose requested asset differs from their current Sound.
		/// [JP] 要求中のアセットと現在の Sound が異なる音源だけを解決する。
		for (EntityID id : world.GetComponents<AudioSource>())
		{
			Actor actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			AudioSource* audioSource = world.GetComponent<AudioSource>(actor.GetEntity());
			if (!audioSource || !audioSource->Pending())
			{
				continue;
			}

			if (audioSource->soundID_ == 0)
			{
				audioSource->Build(nullptr);
				continue;
			}

			Handle<Sound> handle = audioResource->GetHandle(audioSource->soundID_);
			if (handle.empty())
			{
				continue;
			}

			Sound* sound = audioResource->Resolve(loader, handle);
			if (!sound)
			{
				continue;
			}

			audioSource->Build(sound);
		}
	}

	/**
	* [EN]
	* Updates active listeners and sources and stops inactive sources.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アクティブなリスナーと音源を更新し、非アクティブな音源を停止する。
	*/
	void AudioSystem::Update(World& world, Float deltaTime)
	{
		/// [EN] Apply every active listener to the shared audio context.
		/// [JP] アクティブな各リスナーを共有の音響環境へ反映する。
		for (EntityID id : world.GetComponents<AudioListener>())
		{
			Actor actor = world.GetActor(id);
			if (!actor || !actor.Active())
			{
				continue;
			}

			AudioListener* audioListener = world.GetComponent<AudioListener>(actor.GetEntity());
			if (!audioListener)
			{
				continue;
			}

			audioListener->Apply(deltaTime);
		}

		/// [EN] Stop inactive sources and apply runtime settings to active sources.
		/// [JP] 非アクティブな音源を停止し、アクティブな音源へ実行時設定を反映する。
		for (EntityID id : world.GetComponents<AudioSource>())
		{
			Actor actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			AudioSource* audioSource = world.GetComponent<AudioSource>(actor.GetEntity());
			if (!audioSource)
			{
				continue;
			}

			if (!actor.Active())
			{
				if (audioSource->Playing())
				{
					audioSource->Stop();
				}
				continue;
			}

			audioSource->Apply(deltaTime);
		}
	}
}
