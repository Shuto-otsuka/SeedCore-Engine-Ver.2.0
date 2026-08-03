#include <GraphicsEngine/System/MovieSystem.h>
#include <GraphicsEngine/Movie/Movie.h>
#include <GraphicsEngine/Movie/MovieResource.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>

namespace SeedCore
{
	void MovieSystem::Update(World& world, ResourceCache& resourceCache)
	{
		MovieResource* movieResource = resourceCache.GetMovieResource();

		Query<Read<Active>, Read<Movie>> query(world);
		query.ForEach([&](EntityID entityID, const Active& active, const Movie& movie)
			{
				if (!active.active_)
				{
					return;
				}

				if (movie.movieID_ == 0 || !movieResource->Contains(movie.movieID_))
				{
					return;
				}

				movieResource->SetLoop(movie.movieID_, movie.loop_);

				if (movie.autoPlay_ && !movieResource->HasAutoPlayStarted(movie.movieID_))
				{
					movieResource->Play(movie.movieID_);
					movieResource->MarkAutoPlayStarted(movie.movieID_);
				}

				movieResource->AdvancePlayback(movie.movieID_);
			});
	}
}
