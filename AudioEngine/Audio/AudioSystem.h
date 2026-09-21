#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class World;
	class ResourceCache;
	struct LoaderSystem;

	/**
	* [EN]
	* Resolves Sound assets for audio-source components and updates active
	* listeners and sources each frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* AudioSource コンポーネントの Sound アセットを解決し、アクティブな
	* リスナーと音源をフレームごとに更新する。
	*/
	class SEEDCORE_API AudioSystem
	{
	public:
		/**
		* [EN]
		* Assigns loaded Sound objects to sources whose asset ID has changed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセット ID が変わった音源へ、読み込み済み Sound を割り当てる。
		*/
		static void ResolveSound(LoaderSystem& loader, ResourceCache& cache, World& world);

		/**
		* [EN]
		* Updates active listeners and sources and stops inactive sources.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アクティブなリスナーと音源を更新し、非アクティブな音源を停止する。
		*/
		static void Update(World& world, Float deltaTime);
	};
}
