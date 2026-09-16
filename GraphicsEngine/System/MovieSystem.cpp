#include <GraphicsEngine/System/MovieSystem.h>
#include <GraphicsEngine/Movie/Movie.h>
#include <GraphicsEngine/Movie/MovieResource.h>
#include <GraphicsEngine/Movie/Video.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>

namespace SeedCore
{
	void MovieSystem::Update(LoaderSystem& loader, World& world, ResourceCache& resourceCache)
	{
		MovieResource* movieResource = resourceCache.GetResource<MovieResource>(AssetType::Movie);

		Query<Read<Active>, Read<Movie>> query(world);
		query.ForEach([&](EntityID entityID, const Active& active, const Movie& movie)
			{
				if (!active.active_)
				{
					return;
				}

				Video* video = movieResource->Resolve(loader, movieResource->GetHandle(movie.movieID_));
				if (movie.movieID_ == 0 || !video)
				{
					return;
				}

				video->SetLoop(movie.loop_);

				if (movie.autoPlay_ && !video->HasAutoPlayStarted())
				{
					video->Play();
					video->MarkAutoPlayStarted();
				}

				video->Update();
			});
	}
}
