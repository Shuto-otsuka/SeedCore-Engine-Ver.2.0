#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

// SeedScriptを継承すると、Unityでいうスクリプト(MonoBehaviour)のようなものが作れます。
// 下のOnAwake/OnStart/OnTick...は実装した関数だけが自動で呼ばれるようになる仕組みなので、
// 使わないものは中身を空にしたままで大丈夫です（削除しても構いません）。

// 使い方: このファイルをコピーしてクラス名を変え、必要な部分だけコメントを外して使ってください。
// ただし一番下のREGISTER_COMPONENTだけは必須です（無いと「コンポーネントを追加」の一覧に出てきません）。

// エンジン側の型（Entity、Position、World、Physics、Input...）は全部 namespace SeedCore の中にあるので、
// 使うときは SeedCore::Entity のように頭に付ける必要があります（下のコードもそうしています）。
// 逆に、この ScTutorial のような自分のスクリプトクラス自体は SeedCore の外（グローバル）に置くのが決まりです。
class ScTutorial :public SeedCore::SeedScript
{
public:
	//=== 基本 ===//

	void OnAwake(); // 生成時に呼ばれる初期化処理

	void OnStart(); // OnAwake 後に呼ばれる初期化処理（他のコンポーネントを参照するならこっち）

	void OnTick(float elapsedTime); // 毎フレーム呼ばれる更新処理

	void OnLateTick(float elapsedTime); // 全員のOnTickが終わった後に呼ばれる更新処理

	void OnFixedTick(float elapsedTime); // 固定時間間隔（既定60Hz、GameTimer::SetFixedDeltaTimeで変更可）で呼ばれる更新処理（物理と足並みを揃えたい処理向け）

	void OnDestroy(); // 破棄される直前に呼ばれる処理

	void OnInspectorGUI(); // インスペクターに独自のImGui UIを描画したいときの処理

	// フィールドに付けるマクロ一覧（何も付けなければインスペクターにもシーンにも出ない、ただの内部変数になります）
	// よく使うのは無印とSC_REFLECTION_FIELD_EXだけで、残りは必要になったら見に来る程度で大丈夫です

	//float demo1_;  // リフレクション&シリアライズなし（内部でしか使わない変数向け）

	//SC_REFLECTION_FIELD()
	//float demo2_;  // インスペクターに変数名そのまま(demo2_)で表示され、シーンにも保存される

	//SC_REFLECTION_FIELD_EX("デモ3")
	//float demo3_;  // インスペクターの表示名を"デモ3"のように指定できる版

	// --- ここから下は状況次第で使う応用系 ---

	//SC_REFLECTION_CLAMPED_EX("デモ4", 0.0f, 1.0f)
	//float demo4_;  // 表示名付き、かつインスペクターで0.0f～1.0fにクランプされる版

	//SC_REFLECTION_FIELD_CONDITION(demo4_ == 0.0f)
	//SC_REFLECTION_FIELD_EX("デモ5")
	//float demo5_;  // demo4_ が0.0fのときだけインスペクターに表示される（条件は自分より前のフィールドを参照する）

	//SC_SERIALIZE_FIELD()
	//float demo6_;  // シーンには保存されるが、インスペクターには表示されない（コードからだけ触りたい値向け）

	//SC_PAYLOAD_FIELD_EX("参照テクスチャ", Texture)
	//SeedCore::Uint32 demo7_;  // インスペクターにドラッグ&ドロップでアセットを設定できるフィールド（中身はアセットID）。Texture/Model/Prefab等、PayloadAssetType.hを参照

	//=== Physics ===//

	void OnCollisionEnter(SeedCore::Entity entity); // Entity に衝突した瞬間の処理（非トリガーのCollider/Rigidbodyが必要）

	void OnCollisionStay(SeedCore::Entity entity); // Entity に衝突している間、毎フレームの処理

	void OnCollisionExit(SeedCore::Entity entity); // Entity との衝突が終わった瞬間の処理

	void OnTriggerEnter(SeedCore::Entity entity); // Entity のコライダー範囲内に入った瞬間の処理（トリガー用Collider/Rigidbodyが必要）

	void OnTriggerStay(SeedCore::Entity entity); // Entity のコライダー範囲内にいる間、毎フレームの処理

	void OnTriggerExit(SeedCore::Entity entity); // Entity のコライダー範囲内から出た瞬間の処理
};
//REGISTER_COMPONENT(ScTutorial); // コンポーネントを登録。これがないと「コンポーネントを追加」の一覧に出てこない。
//REGISTER_COMPONENT(ScTutorial, "カテゴリ名"); // 一覧内のカテゴリ名を指定したい場合はこちら（省略時は"Custom"）
