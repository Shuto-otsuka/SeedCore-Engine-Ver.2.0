#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct Movie
	{
		enum class DisplayMode
		{
			Fullscreen,
			Sprite,
			Billboard,
		};

		SC_PAYLOAD_FIELD_EX("動画ID", Movie)
		Uint32 movieID_ = 0;

		SC_REFLECTION_FIELD_EX("表示形式")
		DisplayMode displayMode_ = DisplayMode::Fullscreen;

		SC_REFLECTION_FIELD_EX("自動再生")
		Bool autoPlay_ = true;

		SC_REFLECTION_FIELD_EX("ループ再生")
		Bool loop_ = true;

		SC_REFLECTION_FIELD_EX("色")
		Color color_ = { 1,1,1,1 };

		SC_REFLECTION_FIELD_CONDITION(displayMode_ != DisplayMode::Fullscreen)
		SC_REFLECTION_FIELD_EX("サイズ")
		Vector2 size_ = { 0,0 };

		SC_REFLECTION_FIELD_CONDITION(displayMode_ != DisplayMode::Fullscreen)
		SC_REFLECTION_FIELD_EX("中心点")
		Vector2 pivot_ = { 0,0 };

		SC_REFLECTION_FIELD_CONDITION(displayMode_ == DisplayMode::Billboard)
		SC_REFLECTION_FIELD_EX("カメラ正対")
		Bool faceCamera_ = false;
	};
	REGISTER_COMPONENT(Movie, "Texture", ComponentStorage::Archetype);
}
