#include <FoundationEngine/ECS/LayerRegistry.h>
#include <FoundationEngine/ECS/LayerCollisionMatrix.h>
#include <FoundationEngine/Serialization/Json/JsonArchive.h>

namespace SeedCore
{
	/**
	* [EN]
	* Renames the layer slot at index (no-op if index is out of range).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* index のレイヤースロットの名前を変更する（index が範囲外なら何も
	* しない）。
	*/
	void LayerRegistry::SetName(Size index, String name)
	{
		if (index >= LayerCount)
		{
			return;
		}

		Names()[index] = std::move(name);
	}

	/**
	* [EN]
	* Returns the name of the layer slot at index (empty only if index is
	* out of range - every in-range slot always has a name).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* index のレイヤースロットの名前を返す（範囲外の場合のみ空文字列 -
	* 範囲内のスロットは常に名前を持つ）。
	*/
	const String& LayerRegistry::GetName(Size index)
	{
		static const String empty;

		if (index >= LayerCount)
		{
			return empty;
		}

		return Names()[index];
	}

	/**
	* [EN]
	* Returns the index of the layer slot named name, or InvalidIndex if
	* no slot has that name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* name という名前のレイヤースロットのインデックスを返す。該当する
	* スロットが無ければ InvalidIndex を返す。
	*/
	Size LayerRegistry::Find(const String& name)
	{
		DynamicArray<String>& names = Names();
		for (Size index = 0; index < names.size(); ++index)
		{
			if (names[index] == name)
			{
				return index;
			}
		}
		return InvalidIndex;
	}

	/**
	* [EN]
	* Returns whether index names a slot with a non-empty name (true for
	* every in-range slot under normal use, since unrenamed slots already
	* default to a placeholder "Layer N" name).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* index が空でない名前を持つスロットを指しているかを返す（通常の
	* 使用では範囲内の全スロットで true - リネームされていないスロットも
	* 既にプレースホルダーの"Layer N"という名前を持つため）。
	*/
	Bool LayerRegistry::IsUsed(Size index)
	{
		if (index >= LayerCount)
		{
			return false;
		}

		return !Names()[index].view().empty();
	}

	/**
	* [EN]
	* Returns every layer slot's name, indexed by slot index (every
	* in-range slot has a name).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全レイヤースロットの名前を、スロットインデックスでアクセスできる
	* 形で返す（範囲内のスロットは常に名前を持つ）。
	*/
	const DynamicArray<String>& LayerRegistry::GetNames()
	{
		return Names();
	}

	/**
	* [EN]
	* Returns the backing name array, lazily initializing it (DefaultLayer
	* starts named "Default"; every other slot starts named with a
	* placeholder "Layer N") on first use.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部の名前配列を返す。初回使用時に遅延初期化する（DefaultLayer
	* だけ最初から"Default"という名前を持ち、それ以外は全てプレース
	* ホルダーの"Layer N"という名前で始まる）。
	*/
	DynamicArray<String>& LayerRegistry::Names()
	{
		static DynamicArray<String> names = []()
			{
				DynamicArray<String> initial(LayerCount);
				for (Size index = 0; index < LayerCount; ++index)
				{
					initial[index] = PlaceholderName(index);
				}
				return initial;
			}();

		return names;
	}

	/**
	* [EN]
	* Reads layer names and LayerCollisionMatrix's collision entries
	* from a single combined LayerBindings.scg (JSON) at path, replacing
	* both's current in-memory data. No-op if the file doesn't exist or
	* can't be read. Names always end up exactly LayerCount entries long
	* (padded with placeholder "Layer N" names, or truncated, if the
	* file was written under a different LayerCount); LayerCollisionMatrix's
	* entries are left at default instead of being resized if their
	* count doesn't match.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1つにまとまった LayerBindings.scg（JSON、path）から、レイヤー名と
	* LayerCollisionMatrix の衝突エントリを読み込み、両方の現在のメモリ上の
	* データを置き換える。ファイルが存在しない/読み込めない場合は何もしない。
	* 名前は常にちょうど LayerCount 個になる（別の LayerCount で書き出された
	* ファイルの場合は、プレースホルダーの "Layer N" で埋めるか切り詰める）。
	* LayerCollisionMatrix のエントリは、個数が一致しない場合はリサイズせず
	* 既定値のままにする。
	*/
	void LayerRegistry::Load(const std::filesystem::path& path)
	{
		JsonInputArchive archive;
		if (!archive.Read(String(path.string())))
		{
			return;
		}

		DynamicArray<String>& names = Names();
		archive.TryField("names", names);

		if (names.size() != LayerCount)
		{
			names.resize(LayerCount);
		}

		for (Size index = 0; index < LayerCount; ++index)
		{
			if (names[index].view().empty())
			{
				names[index] = PlaceholderName(index);
			}
		}

		LayerCollisionMatrix::Load(archive);
	}

	/**
	* [EN]
	* Writes layer names and LayerCollisionMatrix's collision entries
	* together to path as a single LayerBindings.scg (JSON).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイヤー名と LayerCollisionMatrix の衝突エントリを、1つにまとめて
	* JSON（LayerBindings.scg）として path へ書き込む。
	*/
	void LayerRegistry::Save(const std::filesystem::path& path)
	{
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		JsonOutputArchive archive;
		archive.Field("names", Names());
		LayerCollisionMatrix::Save(archive);

		archive.Write(String(path.string()));
	}

	/**
	* [EN]
	* Returns the name a slot falls back to before it's ever been
	* explicitly renamed - shared between Names()'s lazy init and
	* Load()'s padding of a short/stale array.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スロットが一度も明示的にリネームされていない時のデフォルト名を
	* 返す - Names() の遅延初期化と、Load() が短い/古い配列を埋める
	* 処理の両方で共有する。
	*/
	String LayerRegistry::PlaceholderName(Size index)
	{
		if (index == DefaultLayer)
		{
			return String("Default");
		}

		std::string placeholderName = "Layer " + std::to_string(index);
		return String(std::string_view(placeholderName));
	}
}
