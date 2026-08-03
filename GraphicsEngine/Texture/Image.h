#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct Image
	{
		enum class ViewType
		{
			Sprite,
			Billboard,
		};

		enum class MotionType
		{
			Static,
			Dynamic,
		};

		SC_PAYLOAD_FIELD_EX("テクスチャID", Texture)
		Uint32 textureID_ = 0;

		SC_REFLECTION_FIELD_EX("テクスチャサイズ")
		Vector2 textureSize_;

		SC_REFLECTION_FIELD_EX("テクスチャ位置")
		Vector2 texturePosition_ = { 0,0 };

		SC_REFLECTION_FIELD_EX("中心点")
		Vector2 pivot_ = { 0,0 };

		SC_REFLECTION_FIELD_EX("色")
		Color color_ = { 1,1,1,1 };

		SC_REFLECTION_FIELD_EX("表示形式")
		ViewType viewType_ = ViewType::Sprite;

		SC_REFLECTION_FIELD_CONDITION(viewType_ == ViewType::Billboard)
		SC_REFLECTION_FIELD_EX("カメラ正対")
		Bool faceCamera_ = false;

		SC_REFLECTION_FIELD_EX("テクスチャタイプ")
		MotionType motionType_ = MotionType::Static;

		SC_REFLECTION_FIELD_CONDITION(motionType_ == MotionType::Dynamic)
		SC_REFLECTION_FIELD_EX("スクロール速度")
		Float scrollSpeed_;

		SC_REFLECTION_FIELD_CONDITION(motionType_ == MotionType::Dynamic)
		SC_REFLECTION_CLAMPED_EX("スクロール向き", -1.0f, 1.0f)
		Vector2 scrollDirection_;
	};
	REGISTER_COMPONENT(Image, "Texture", ComponentStorage::Archetype);
}