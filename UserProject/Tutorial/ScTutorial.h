#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

class ScTutorial :public SeedCore::SeedScript
{
public:
	//float demo1_;  // リフレクション&シリアライズなし

	//SC_REFLECTION_FIELD()
	//float demo2_;  // demo2_

	//SC_REFLECTION_FIELD_EX("デモ3")
	//float demo3_;  // デモ3

	//SC_REFLECTION_CLAMPED_EX("デモ4", 0.0f, 1.0f)
	//float demo4_;  // 0.0f～1.0f

	//SC_REFLECTION_FIELD_CONDITION(demo4_ == 0.0f)
	//SC_REFLECTION_FIELD_EX("デモ5")
	//float demo5_;  // demo4_ が0.0fのときのみ有効

public:
	void OnAwake(); // 生成時に呼ばれる初期化処理

	void OnStart(); // OnAwake 後に呼ばれる初期化処理

	void OnTick(float elapsedTime); // 更新処理

	void OnLateTick(float elapsedTime); // OnTick 後に呼ばれる更新処理

	void OnFixedTick(float elapsedTime); // 固定時間で呼ばれる更新処理

	void OnDestroy(); // 破棄時に呼ばれる処理

	void OnInspectorGUI(); // カスタムImGuiを生成する処理

	void OnCollisionEnter(SeedCore::Entity entity); // Entity に衝突した瞬間の処理

	void OnCollisionStay(SeedCore::Entity entity); // Entity に衝突している間の処理

	void OnCollisionExit(SeedCore::Entity entity); // Entity に衝突し終わった瞬間の処理

	void OnTriggerEnter(SeedCore::Entity entity); // Entity のコライダー範囲内に入った瞬間の処理

	void OnTriggerStay(SeedCore::Entity entity); // Entity にコライダー範囲内にいる間の処理

	void OnTriggerExit(SeedCore::Entity entity); // Entity にコライダー範囲内から出た瞬間の処理
};
//REGISTER_COMPONENT(ScTutorial); // コンポーネントを登録。これがないと一覧に出てこない。