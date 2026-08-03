#include <AudioEngine/Audio/Audio.h>
#include <AudioEngine/CRI/CriManager.h>
#include <FoundationEngine/Resource/Gateway.h>

namespace SeedCore
{
	Audio::Audio() :criManager_(Gateway::GetCriManager())
	{
		/// No Code
	}
}