#include "UserProject/Tutorial/ScTutorial.h"
// 他のコンポーネントを触りたいときは、このように必要なコンポーネントのヘッダーをincludeします
//#include <FoundationEngine/ECS/World.h>
//#include <FoundationEngine/ECS/Component/Position.h>
//#include <FoundationEngine/ECS/Component/Rotation.h>
//#include <FoundationEngine/ECS/Component/Scale.h>
//#include <FoundationEngine/ECS/Component/Velocity.h>
//#include <FoundationEngine/ECS/Component/Name.h>
//#include <PhysicsEngine/Rigidbody/Rigidbody.h>
//#include <PhysicsEngine/Physics/Physics.h>
//#include <GraphicsEngine/Camera/ScreenSpace.h>

// 上の3つ（Rigidbody/Physics/ScreenSpace）のように、まとめヘッダーがまだ無いものは直接includeします。
// 一方、下のSeedCoreプロジェクト直下の Sc○○.h は、正確な内部パスを覚えなくても使えるまとめヘッダーです
//#include <SeedCore/ScInput.h>
//#include <SeedCore/ScMath.h>
//#include <SeedCore/ScScene.h>

//=== 基本 ===//

void ScTutorial::OnAwake()
{
	//demo1_ = 1.0f; // ここでのdemo1_はまだ初期値のまま。生成直後の下準備に使う
}

void ScTutorial::OnStart()
{
	// GetWorld()/GetActor()はSeedScript(ComponentBase)が持っている関数で、自分がアタッチされているWorld/Actorが取れる
	//auto& world = GetWorld();
	//SeedCore::Entity entity = GetActor().GetEntity();

	// World::GetComponent<T>(entity)で、同じActorが持つ他のコンポーネントへのポインタが取れる（無ければnullptr）
	//SeedCore::Position* position = world.GetComponent<SeedCore::Position>(entity);
	//SeedCore::Rotation* rotation = world.GetComponent<SeedCore::Rotation>(entity);
	//SeedCore::Scale* scale = world.GetComponent<SeedCore::Scale>(entity);
	//SeedCore::Velocity* velocity = world.GetComponent<SeedCore::Velocity>(entity);
	//
	// 例: 開始位置を(0, 1, 0)にセットする
	//position->x = 0;
	//position->y = 1;
	//position->z = 0;

	// 名前でActorを探したいときはWorld::GetActor(名前)。ただしEntityID/永続ID版と違って索引が無くO(actor数)なので、
	// 毎フレーム呼ぶよりOnStart等で一度だけ探してポインタを保持しておくのがおすすめ
	//SeedCore::Actor* targetActor = world.GetActor(SeedCore::String("Player"));
}

void ScTutorial::OnTick(float elapsedTime)
{
	// elapsedTimeは前フレームからの経過秒数。毎フレーム動く処理はここに書く

	// キー入力を直接見たいときはInputSystem::KeyStateを使う。第2引数は既定でIsPressed(押しっぱなし判定)、
	// OnPressed(押した瞬間だけ)/OnReleased(離した瞬間だけ)も指定できる
	//if (SeedCore::InputSystem::KeyState(SeedCore::InputSystem::Key::Space, SeedCore::InputSystem::OnPressed))
	//{
	//}

	// ただしキーを直書きすると後から割り当てを変えづらいので、基本は「アクション」名で判定するのがおすすめ
	// （アクション⇔キー/ゲームパッドボタンの対応はエディターの「入力設定」で編集・保存できる）
	//if (SeedCore::InputSystem::ActionState("Jump", SeedCore::InputSystem::OnPressed))
	//{
	//}

	// 移動のような2軸入力は ActionAxis2D が便利。WASD/矢印キー/アナログスティックのどれが押されていても
	// 同じ1つのVector2として返ってくるので、キーボード/ゲームパッドを呼び出し側で分岐する必要が無い
	//SeedCore::Vector2 moveInput = SeedCore::InputSystem::ActionAxis2D("Move");
}

void ScTutorial::OnLateTick(float elapsedTime)
{
	// 全Actor・全コンポーネントのOnTickが終わった後に呼ばれる。
	// 例えばカメラがキャラを追いかけるような、「他の誰かの今フレームの結果」を見てから動きたい処理向け
}

void ScTutorial::OnFixedTick(float elapsedTime)
{
	// こちらのelapsedTimeは常に一定の値（既定1/60秒=60Hz）。物理演算と同じ頻度で呼ばれる
}

void ScTutorial::OnDestroy()
{
	//demo1_ = 0.0f; // 破棄される前に後片付けしたい処理（確保したリソースの解放など）はここに書く
}

void ScTutorial::OnInspectorGUI()
{
	// ここはFoundationEngine/Prelude.h経由でImGuiが使えるので、普通のImGui関数がそのまま書ける
	//ImGui::Text("デモ");
}

//=== Physics ===//

// --- 移動/ピッキング（下のコライダーコールバックとは別の話、OnTick等から使う想定） ---

// 物理的に動かしたいときは、Positionを直接書き換えるのではなくRigidbodyを使う（Rigidbodyが毎フレーム物理演算の結果をPositionへ書き戻す）
//SeedCore::Rigidbody* rigidbody = world.GetComponent<SeedCore::Rigidbody>(entity);

// マウスでクリックしたところにある物を拾いたい（マウスピッキング）ときは、
// ScreenSpace::ScreenToWorld でスクリーン座標をワールド空間のRayに変換してからRaycastする。
// ScreenToWorldは常にゲーム自身のアクティブなCameraを基準にするので、エディターのゲームビューでも
// 単体で実行したRuntimeでも同じコードで動く（渡す引数もInputSystem::MouseX()/MouseY()だけで、view/projection等は不要）。
//SeedCore::Ray mouseRay = SeedCore::ScreenSpace::ScreenToWorld(SeedCore::Vector2(SeedCore::InputSystem::MouseX(), SeedCore::InputSystem::MouseY()));
//SeedCore::RaycastHit hit;
//if (GetActor().GetPhysics().Raycast(mouseRay.origin_, mouseRay.direction_, 1000.0f, hit))
//{
//	SeedCore::Actor* pickedActor = GetWorld().GetActor(hit.entityID_);
//}

// --- コライダーコールバック ---

void ScTutorial::OnCollisionEnter(SeedCore::Entity entity)
{
	// entity は衝突してきた相手のEntity。GetWorld().GetActor(entity)で相手のActorが取れる
	//SeedCore::Actor* otherActor = GetWorld().GetActor(entity);

	// 相手が何なのかを判定したいときは、名前で分岐するよりタグ/レイヤーで判定するのがおすすめ
	// タグ=1つのActorに複数付けられる、ゆるいカテゴリ分け用（"Enemy"かつ"Flying"のように重ねられる）
	// レイヤー=1つのActorに1つだけ、主に物理の衝突フィルタ（LayerSettingsPanelの衝突マトリクス）用
	// どちらもインスペクターで設定する
	//if (otherActor && otherActor->HasTag("Enemy"))
	//{
	//}
	//
	//if (otherActor && otherActor->GetLayerName() == "Enemy")
	//{
	//}
}

void ScTutorial::OnCollisionStay(SeedCore::Entity entity)
{

}

void ScTutorial::OnCollisionExit(SeedCore::Entity entity)
{

}

void ScTutorial::OnTriggerEnter(SeedCore::Entity entity)
{

}

void ScTutorial::OnTriggerStay(SeedCore::Entity entity)
{

}

void ScTutorial::OnTriggerExit(SeedCore::Entity entity)
{

}

//=== Scene ===//

// シーンを切り替えたいときはSceneクラスの静的関数を使う。
// SceneTransitionSystemを直接使う場合はWorld/ResourceCache/JobExecutorが要るが、
// Sceneの static Change()/Update() はプロセス全体の参照を(Editor.cpp/Runtime側で)束縛済みなので、
// SeedScriptのOnTick等からでも、Worldを一切意識せず呼べる。
//
// 即座に切り替える場合
//SeedCore::Scene::Change("Scene/NextScene.scene");
//
// フェードアウト/フェードインしながら非同期に切り替える場合
//SeedCore::Scene::Change("Scene/NextScene.scene", 0.3f, 0.3f);
//
// ローディングシーンを挟んで非同期に切り替える場合
//SeedCore::Scene::Change("Scene/NextScene.scene", "Scene/LoadingScene.scene");
//
// Scene::Update(deltaTime)は遷移の状態機械を進める処理で、こちらは呼ぶ必要はない（Editor.cpp側で毎フレーム呼ばれている）
// 注意: Runtime側はまだ配線されていないので、Runtime実行ファイルではScene::Changeが今のところ動かない
//
// パスからAssetIDを引きたいとき（PrefabやTexture等、他の何かにアセットIDを渡す前段として）はScene::GetAssetを使う
// これもSceneと同じ束縛済みのResourceCache経由で解決するので、SeedScriptから直接呼べる
//SeedCore::Uint32 targetSceneAssetID = SeedCore::Scene::GetAsset("Scene/NextScene.scene");