#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class CriManager;

	class Audio
	{
	public:
		Audio();
		~Audio() = default;

	private:
		CriManager& criManager_;
	};
}