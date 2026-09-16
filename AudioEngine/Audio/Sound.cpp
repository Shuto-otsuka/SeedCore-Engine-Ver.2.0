#include <AudioEngine/Audio/Sound.h>

namespace SeedCore
{
	SoundType Sound::Type()const
	{
		return type_;
	}

	CriAtomExAcbHn Sound::AcbHandle()const
	{
		return acbHandle_;
	}

	void* Sound::Data()
	{
		return data_.data();
	}

	Size Sound::DataSize()const
	{
		return data_.size();
	}

	const String& Sound::AwbPath()const
	{
		return awbPath_;
	}
}
