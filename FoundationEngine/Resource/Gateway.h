#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class JoltManager;
	class CriManager;
	class FontManager;
	class EffekseerManager;

	/**
	* [EN]
	* Static bridge exposing the process-wide JoltManager (physics) and
	* CriManager (audio) instances to FoundationEngine-level code (e.g.
	* Resource/ loaders), which cannot depend directly on PhysicsEngine
	* or AudioEngine. Runtime/Editor bind the concrete managers once at
	* startup via BindJoltManager/BindCriManager; everything else
	* retrieves them through this class instead of holding its own reference.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* PhysicsEngine/AudioEngine に直接依存できない FoundationEngine
	* レベルのコード（Resource/ のローダーなど）へ、プロセス全体の
	* JoltManager（物理）と CriManager（オーディオ）インスタンスを
	* 公開する静的ブリッジ。Runtime/Editor が起動時に一度
	* BindJoltManager/BindCriManager 経由で具体的なマネージャを
	* 束縛し、それ以外のコードは自前の参照を保持する代わりにこのクラス
	* 経由で取得する。
	*/
	class SEEDCORE_API Gateway :public NonTransferable
	{
	public:
		/**
		* [EN]
		* Binds the process-wide JoltManager instance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プロセス全体の JoltManager インスタンスを束縛する。
		*/
		static void BindJoltManager(JoltManager* manager);

		/**
		* [EN]
		* Binds the process-wide CriManager instance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プロセス全体の CriManager インスタンスを束縛する。
		*/
		static void BindCriManager(CriManager* manager);

		/**
		* [EN]
		* Binds the process-wide FontManager instance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プロセス全体の FontManager インスタンスを束縛する。
		*/
		static void BindFontManager(FontManager* manager);

		/**
		* [EN]
		* Binds the process-wide EffekseerManager instance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プロセス全体の EffekseerManager インスタンスを束縛する。
		*/
		static void BindEffekseerManager(EffekseerManager* manager);

		/**
		* [EN]
		* Returns the bound JoltManager instance (must have been bound
		* via BindJoltManager beforehand).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 束縛済みの JoltManager インスタンスを返す（事前に
		* BindJoltManager で束縛されている必要がある）。
		*/
		static JoltManager& GetJoltManager();

		/**
		* [EN]
		* Returns the bound CriManager instance (must have been bound
		* via BindCriManager beforehand).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 束縛済みの CriManager インスタンスを返す（事前に
		* BindCriManager で束縛されている必要がある）。
		*/
		static CriManager& GetCriManager();

		/**
		* [EN]
		* Returns the bound FontManager instance (must have been bound via
		* BindFontManager beforehand).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 束縛済みの FontManager インスタンスを返す（事前に
		* BindFontManager で束縛されている必要がある）。
		*/
		static FontManager& GetFontManager();

		/**
		* [EN]
		* Returns the bound EffekseerManager instance (must have been bound
		* via BindEffekseerManager beforehand).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 束縛済みの EffekseerManager インスタンスを返す（事前に
		* BindEffekseerManager で束縛されている必要がある）。
		*/
		static EffekseerManager& GetEffekseerManager();

	private:
		/// [EN] The bound process-wide JoltManager instance, or nullptr if not yet bound.
		/// [JP] 束縛済みのプロセス全体の JoltManager インスタンス。まだ束縛されていなければ nullptr。
		static JoltManager* joltManager_;

		/// [EN] The bound process-wide CriManager instance, or nullptr if not yet bound.
		/// [JP] 束縛済みのプロセス全体の CriManager インスタンス。まだ束縛されていなければ nullptr。
		static CriManager* criManager_;

		/// [EN] The bound process-wide FontManager instance, or nullptr if not yet bound.
		/// [JP] 束縛済みのプロセス全体の FontManager インスタンス。まだ束縛されていなければ nullptr。
		static FontManager* fontManager_;

		/// [EN] The bound process-wide EffekseerManager instance, or nullptr if not yet bound.
		/// [JP] 束縛済みのプロセス全体の EffekseerManager インスタンス。まだ束縛されていなければ nullptr。
		static EffekseerManager* effekseerManager_;
	};
}
