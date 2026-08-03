#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class SkyLight :public SeedScript
	{
	public:
		enum class CaptureType
		{
			Lietime,
			Realtime,
		};

	public:
		SC_REFLECTION_FIELD_EX("スカイマップ使用")
		Bool useSkymap_ = false;

		SC_REFLECTION_FIELD_CONDITION(useSkymap_)
		SC_PAYLOAD_FIELD_EX("スカイマップID", Sky)
		Uint32 skymapID_ = 0;

		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 1.0f)
		Float intensity_ = 1.0f;

		SC_REFLECTION_FIELD_EX("キャプチャータイプ")
		CaptureType captureType_ = CaptureType::Lietime;

		SC_REFLECTION_FIELD_CONDITION(captureType_ == CaptureType::Realtime)
		SC_REFLECTION_CLAMPED_EX("キャプチャー間隔", 0.001f, 100.0f)
		Float captureDuration_ = 1.0f;

	public:
		void OnInspectorGUI()
		{
			ImGui::Text("注: キャプチャータイプ Realtime は非推奨。");
			ImGui::Text("    レイトレーシング反射を有効にすることを推奨します。");
		}
	};
	REGISTER_COMPONENT(SkyLight, "Light");
}
