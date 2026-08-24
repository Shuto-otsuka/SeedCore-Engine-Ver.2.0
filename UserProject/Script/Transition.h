#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

class Transition :public SeedCore::SeedScript
{
public:
	void OnStart(); // 開始時に呼ばれる初期化処理

	void OnTick(float elapsedTime); // 更新処理
};
REGISTER_COMPONENT(Transition);
