#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct Text
	{
		enum class ViewType
		{
			Sprite,
			Billboard,
		};

		SC_PAYLOAD_FIELD_EX("フォントID", Font)
		Uint32 fontID_ = 0;

		SC_REFLECTION_FIELD_EX("テキスト")
		String text_;

		SC_REFLECTION_FIELD_EX("文字サイズ")
		Float fontSize_ = 32.0f;

		SC_REFLECTION_FIELD_EX("文字間隔")
		Float letterSpacing_ = 0.0f;

		SC_REFLECTION_FIELD_EX("行間")
		Float lineSpacing_ = 1.0f;

		SC_REFLECTION_FIELD_EX("中心点")
		Vector2 pivot_ = { 0,0 };

		SC_REFLECTION_FIELD_EX("色")
		Color color_ = { 1,1,1,1 };

		SC_REFLECTION_CLAMPED_EX("アウトライン幅", 0.0f, 28.0f)
		Float outlineWidth_ = 0.0f;

		SC_REFLECTION_FIELD_EX("アウトライン色")
		Color outlineColor_ = { 0,0,0,1 };

		SC_REFLECTION_CLAMPED_EX("発光量", 0.0f, 8.0f)
		Float glowPower_ = 0.0f;

		SC_REFLECTION_FIELD_EX("発光色")
		Color glowColor_ = { 1,1,1,1 };

		SC_REFLECTION_FIELD_EX("表示形式")
		ViewType viewType_ = ViewType::Sprite;

		SC_REFLECTION_FIELD_CONDITION(viewType_ == ViewType::Billboard)
		SC_REFLECTION_FIELD_EX("カメラ正対")
		Bool faceCamera_ = false;

		SC_REFLECTION_FIELD_EX("影")
		Bool shadowEnable_ = false;

		SC_REFLECTION_FIELD_CONDITION(shadowEnable_)
		SC_REFLECTION_FIELD_EX("影オフセット")
		Vector2 shadowOffset_ = { 2,2 };

		SC_REFLECTION_FIELD_CONDITION(shadowEnable_)
		SC_REFLECTION_FIELD_EX("影色")
		Color shadowColor_ = { 0,0,0,0.5f };
	};
	REGISTER_COMPONENT(Text, "Texture", ComponentStorage::Archetype);
}
