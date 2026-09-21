#include <AudioEngine/Audio/MixerSystem.h>
#include <AudioEngine/CRI/CriManager.h>
#include <FoundationEngine/Resource/Gateway.h>

namespace SeedCore
{
	/**
	* [EN]
	* Sets the master volume multiplier.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* マスター音量の倍率を設定する。
	*/
	void MixerSystem::MasterVolume(Float volume)
	{
		Gateway::GetCriManager().MasterVolume(volume);
	}

	/**
	* [EN]
	* Sets the volume multiplier for a named category.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定したカテゴリの音量倍率を設定する。
	*/
	void MixerSystem::CategoryVolume(const String& category, Float volume)
	{
		Gateway::GetCriManager().CategoryVolume(category, volume);
	}

	/**
	* [EN]
	* Restores all volume settings to their defaults.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* すべての音量設定を既定値へ戻す。
	*/
	void MixerSystem::ResetVolume()
	{
		Gateway::GetCriManager().ResetVolume();
	}

	/**
	* [EN]
	* Saves the current volume bindings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の音量設定を保存する。
	*/
	Bool MixerSystem::Save()
	{
		return Gateway::GetCriManager().Save();
	}

	/**
	* [EN]
	* Returns the current master volume multiplier.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在のマスター音量倍率を返す。
	*/
	Float MixerSystem::MasterVolume()
	{
		return Gateway::GetCriManager().MasterVolume();
	}

	/**
	* [EN]
	* Returns the current volume multiplier for a named category.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定したカテゴリの現在の音量倍率を返す。
	*/
	Float MixerSystem::CategoryVolume(const String& category)
	{
		return Gateway::GetCriManager().CategoryVolume(category);
	}

	/**
	* [EN]
	* Returns the names of the available mixer categories.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 利用可能なミキサーカテゴリ名を返す。
	*/
	const DynamicArray<String>& MixerSystem::CategoryNameList()
	{
		return Gateway::GetCriManager().CategoryNameList();
	}
}
