#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class SEEDCORE_API MeshCollision :public NonCopyable
	{
	private:
		friend class MeshCollisionLoader;

		DynamicArray<Vector3> positions_;
		DynamicArray<Uint32> indices_;

	public:
		MeshCollision() = default;
		~MeshCollision() = default;

		MeshCollision(MeshCollision&&)noexcept = default;
		MeshCollision& operator=(MeshCollision&&)noexcept = default;

		[[nodiscard]] const DynamicArray<Vector3>& Positions()const;
		[[nodiscard]] const DynamicArray<Uint32>& Indices()const;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("positions", positions_),
				cereal::make_nvp("indices", indices_)
			);
		}
	};
}
