#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	class EffekseerMemoryFileReader :public Effekseer::FileReader
	{
	public:
		explicit EffekseerMemoryFileReader(DynamicArray<Byte> data);
		~EffekseerMemoryFileReader()override = default;

		Size Read(void* buffer, Size size)override;

		void Seek(Int position)override;

		Int GetPosition()const override;

		Size GetLength()const override;

	private:
		DynamicArray<Byte> data_;

		Int position_ = 0;
	};

	/**
	* [EN]
	* Effekseer FileInterface that resolves a path from an in-memory
	* registry (populated from a ".effekseer" cache's embedded
	* dependencies) before falling back to a real disk read. Lets a
	* shipped build carry only ".effekseer" files without the original
	* ".efkmodel"/texture assets on disk.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* パスを("`.effekseer`"キャッシュに埋め込まれた依存関係で満たされる)
	* インメモリレジストリからまず解決し、無ければ実ディスク読み込みに
	* フォールバックするEffekseerのFileInterface。出荷ビルドが元の
	* ".efkmodel"/テクスチャアセットをディスクに持たず ".effekseer" だけを
	* 持てるようにする。
	*/
	class EffekseerFileInterface :public Effekseer::FileInterface
	{
	public:
		EffekseerFileInterface() = default;
		~EffekseerFileInterface()override = default;

		void Register(const std::u16string& path, DynamicArray<Byte> data);

		void Clear();

		Effekseer::FileReaderRef OpenRead(const Char16* path)override;

		Effekseer::FileWriterRef OpenWrite(const Char16* path)override;

	private:
		FlatMap<std::u16string, DynamicArray<Byte>> registry_;

		Effekseer::DefaultFileInterface diskFallback_;
	};
}
