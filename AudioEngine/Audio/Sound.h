#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	enum class SoundType :Uint32
	{
		CueSheet,
		Wave,
	};

	class SEEDCORE_API Sound :public NonCopyable
	{
	private:
		friend class AudioLoader;

	public:
		Sound() = default;
		~Sound() = default;

		Sound(Sound&&)noexcept = default;
		Sound& operator=(Sound&&)noexcept = default;

	public:
		[[nodiscard]] SoundType Type()const;

		[[nodiscard]] CriAtomExAcbHn AcbHandle()const;

		[[nodiscard]] void* Data();

		[[nodiscard]] Size DataSize()const;

		[[nodiscard]] const String& AwbPath()const;

	private:

		SoundType type_ = SoundType::Wave;

		CriAtomExAcbHn acbHandle_ = nullptr;

		DynamicArray<Byte> data_;

		String awbPath_;
	};
}
