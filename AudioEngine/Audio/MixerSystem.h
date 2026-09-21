#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Static interface for controlling and persisting the engine's master and
	* category volume settings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンジンのマスター音量とカテゴリ音量を制御し、保存するための
	* static インターフェース。
	*/
	class SEEDCORE_API MixerSystem
	{
	public:
		/**
		* [EN]
		* Sets the master volume multiplier.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マスター音量の倍率を設定する。
		*/
		static void MasterVolume(Float volume);

		/**
		* [EN]
		* Sets the volume multiplier for a named category.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定したカテゴリの音量倍率を設定する。
		*/
		static void CategoryVolume(const String& category, Float volume);

		/**
		* [EN]
		* Restores all volume settings to their defaults.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* すべての音量設定を既定値へ戻す。
		*/
		static void ResetVolume();

		/**
		* [EN]
		* Saves the current volume bindings.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の音量設定を保存する。
		*/
		static Bool Save();

	public:
		/**
		* [EN]
		* Returns the current master volume multiplier.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在のマスター音量倍率を返す。
		*/
		[[nodiscard]] static Float MasterVolume();

		/**
		* [EN]
		* Returns the current volume multiplier for a named category.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定したカテゴリの現在の音量倍率を返す。
		*/
		[[nodiscard]] static Float CategoryVolume(const String& category);

		/**
		* [EN]
		* Returns the names of the available mixer categories.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 利用可能なミキサーカテゴリ名を返す。
		*/
		[[nodiscard]] static const DynamicArray<String>& CategoryNameList();
	};
}
