#include <FoundationEngine/Resource/Gateway.h>

#include <PhysicsEngine/JoltPhysics/JoltManager.h>
#include <AudioEngine/CRI/CriManager.h>
#include <GraphicsEngine/Font/FontManager.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerManager.h>
#include <GraphicsEngine/DLSS/DlssManager.h>

namespace SeedCore
{
	JoltManager* Gateway::joltManager_ = nullptr;
	CriManager* Gateway::criManager_ = nullptr;
	FontManager* Gateway::fontManager_ = nullptr;
	EffekseerManager* Gateway::effekseerManager_ = nullptr;
	DlssManager* Gateway::dlssManager_ = nullptr;

	/**
	* [EN]
	* Binds the process-wide JoltManager instance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体の JoltManager インスタンスを束縛する。
	*/
	void Gateway::BindJoltManager(JoltManager* manager)
	{
		joltManager_ = manager;
	}

	/**
	* [EN]
	* Binds the process-wide CriManager instance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体の CriManager インスタンスを束縛する。
	*/
	void Gateway::BindCriManager(CriManager* manager)
	{
		criManager_ = manager;
	}

	/**
	* [EN]
	* Binds the process-wide FontManager instance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体の FontManager インスタンスを束縛する。
	*/
	void Gateway::BindFontManager(FontManager* manager)
	{
		fontManager_ = manager;
	}

	/**
	* [EN]
	* Binds the process-wide EffekseerManager instance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体の EffekseerManager インスタンスを束縛する。
	*/
	void Gateway::BindEffekseerManager(EffekseerManager* manager)
	{
		effekseerManager_ = manager;
	}

	/**
	* [EN]
	* Binds the process-wide DlssManager instance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体の DlssManager インスタンスを束縛する。
	*/
	void Gateway::BindDlssManager(DlssManager* manager)
	{
		dlssManager_ = manager;
	}

	/**
	* [EN]
	* Returns the bound JoltManager instance (must have been bound via
	* BindJoltManager beforehand).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 束縛済みの JoltManager インスタンスを返す（事前に BindJoltManager
	* で束縛されている必要がある）。
	*/
	JoltManager& Gateway::GetJoltManager()
	{
		return *joltManager_;
	}

	/**
	* [EN]
	* Returns the bound CriManager instance (must have been bound via
	* BindCriManager beforehand).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 束縛済みの CriManager インスタンスを返す（事前に BindCriManager
	* で束縛されている必要がある）。
	*/
	CriManager& Gateway::GetCriManager()
	{
		return *criManager_;
	}

	/**
	* [EN]
	* Returns the bound FontManager instance (must have been bound via
	* BindFontManager beforehand).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 束縛済みの FontManager インスタンスを返す（事前に BindFontManager
	* で束縛されている必要がある）。
	*/
	FontManager& Gateway::GetFontManager()
	{
		return *fontManager_;
	}

	/**
	* [EN]
	* Returns the bound EffekseerManager instance (must have been bound via
	* BindEffekseerManager beforehand).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 束縛済みの EffekseerManager インスタンスを返す（事前に
	* BindEffekseerManager で束縛されている必要がある）。
	*/
	EffekseerManager& Gateway::GetEffekseerManager()
	{
		return *effekseerManager_;
	}

	/**
	* [EN]
	* Returns the bound DlssManager instance (must have been bound via
	* BindDlssManager beforehand).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 束縛済みの DlssManager インスタンスを返す（事前に BindDlssManager
	* で束縛されている必要がある）。
	*/
	DlssManager& Gateway::GetDlssManager()
	{
		return *dlssManager_;
	}
}
