#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

class Test :public SeedCore::SeedScript
{
public:
	void OnStart(); // 開始時に呼ばれる初期化処理

	void OnTick(float elapsedTime); // 更新処理

public:
	SC_REFLECTION_FIELD_EX("テスト")
		bool test_ = false;
};
REGISTER_COMPONENT(Test);
