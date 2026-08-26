#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class MovieLoader
	{
	public:
		MovieLoader() = default;

		~MovieLoader();

		Bool Initialize(const std::string& filePath);

		void Finalize();

		Bool ReadNextSample(Double& outTimestampSeconds);

		Bool Seek(Double timeSeconds);

		[[nodiscard]] const Byte* GetPixelData()const;

		[[nodiscard]] Int GetWidth()const;

		[[nodiscard]] Int GetHeight()const;

		[[nodiscard]] Longlong GetRowPitch()const;

		[[nodiscard]] Double GetDuration()const;

		[[nodiscard]] Bool IsEndOfStream()const;

	private:
		Microsoft::WRL::ComPtr<IMFSourceReader> sourceReader_;

		Microsoft::WRL::ComPtr<IMFByteStream> byteStream_;

		DynamicArray<Byte> pixelBuffer_;

		Int width_ = 0;

		Int height_ = 0;

		Longlong rowPitch_ = 0;

		Double duration_ = 0.0;

		Bool endOfStream_ = false;

		Bool initialized_ = false;
	};
}
