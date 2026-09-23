#include "UserProject/Tutorial/ScTutorial.h"

// SeedCoreプロジェクト直下の Sc○○.h は、正確な内部パスを覚えなくても使えるまとめヘッダーです。
// 必要なものだけコメントを外してください（ScAll.h だけは全部入りなので、迷ったときや試作向け）。
//#include <SeedCore/ScComponent.h>  // Position/Rotation/Rigidbody/Camera... コンポーネント全部 + Actor/World/Query
//#include <SeedCore/ScInput.h>      // キーボード/マウス/ゲームパッド
//#include <SeedCore/ScMath.h>       // Vector/Matrix/Quaternion/Ray/乱数
//#include <SeedCore/ScPhysics.h>    // Raycast などの物理問い合わせ
//#include <SeedCore/ScPrefab.h>     // Prefabの実体化
//#include <SeedCore/ScScene.h>      // シーン切り替え
//#include <SeedCore/ScScreen.h>     // スクリーン座標 ⇔ ワールド座標
//#include <SeedCore/ScLog.h>        // ログ出力（エディターのログパネルに出る）
//#include <SeedCore/ScAll.h>        // 上の全部

// まとめヘッダーに無いものを使いたいときは、そのヘッダーを直接includeします
//#include <GraphicsEngine/Model/Animation/Animator.h>

//=== 基本 ===//

void ScTutorial::OnAwake()
{
	//demo1_ = 1.0f; // ここでのdemo1_はまだ初期値のまま。生成直後の下準備に使う
}

void ScTutorial::OnStart()
{
	// GetWorld()/GetActor()はSeedScript(ComponentBase)が持っている関数で、自分がアタッチされているWorld/Actorが取れる
	//SeedCore::World& world = GetWorld();
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
	// 毎フレーム呼ぶよりOnStart等で一度だけ探しておくのがおすすめ。
	// Actorは参照ではなく値（World内のEntityを指す軽い手形）なので、そのまま自分のメンバーに持っておける
	//SeedCore::Actor targetActor = world.GetActor(SeedCore::String("Player"));

	// ログを出したいときは SC_LOG_ 系のマクロ。std::formatと同じ書式で、エディターのログパネルに出る
	//SC_LOG_NOTICE("開始しました: {}", 1);
}

void ScTutorial::OnTick(float elapsedTime)
{
	// elapsedTimeは前フレームからの経過秒数。毎フレーム動く処理はここに書く

	// キー入力を直接見たいときはInputSystem::KeyStateを使う。第2引数のTriggerModeは
	// NONE(既定、押しっぱなし判定) / RISING_EDGE(押した瞬間だけ) / FALLING_EDGE(離した瞬間だけ)
	//if (SeedCore::InputSystem::KeyState(SeedCore::InputSystem::Key::Space, SeedCore::InputSystem::TriggerMode::RISING_EDGE))
	//{
	//}

	// ただしキーを直書きすると後から割り当てを変えづらいので、基本は「アクション」名で判定するのがおすすめ
	// （アクション⇔キー/ゲームパッドボタンの対応はエディターの「入力設定」で編集・保存できる）
	//if (SeedCore::InputSystem::ActionState(SeedCore::String("Jump"), SeedCore::InputSystem::TriggerMode::RISING_EDGE))
	//{
	//}

	// 移動のような2軸入力は ActionAxis2D が便利。WASD/矢印キー/アナログスティックのどれが押されていても
	// 同じ1つのVector2として返ってくるので、キーボード/ゲームパッドを呼び出し側で分岐する必要が無い
	//SeedCore::Vector2 moveInput = SeedCore::InputSystem::ActionAxis2D(SeedCore::String("Move"));
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
//SeedCore::Rigidbody* rigidbody = GetWorld().GetComponent<SeedCore::Rigidbody>(GetActor().GetEntity());

// マウスでクリックしたところにある物を拾いたい（マウスピッキング）ときは、
// ScreenSpace::ScreenToWorld でスクリーン座標をワールド空間のRayに変換してからRaycastする。
// ScreenToWorldは常にゲーム自身のアクティブなCameraを基準にするので、エディターのゲームビューでも
// 単体で実行したRuntimeでも同じコードで動く（渡す引数もInputSystem::MouseX()/MouseY()だけで、view/projection等は不要）。
//SeedCore::Ray mouseRay = SeedCore::ScreenSpace::ScreenToWorld(SeedCore::Vector2(SeedCore::InputSystem::MouseX(), SeedCore::InputSystem::MouseY()));
//SeedCore::RaycastHit hit;
//if (GetActor().GetPhysics().Raycast(mouseRay.origin_, mouseRay.direction_, 1000.0f, hit))
//{
//	// hitには当たった位置(position_)、法線(normal_)、距離(distance_)、相手(entityID_)が入る
//	SeedCore::Actor pickedActor = GetWorld().GetActor(hit.entityID_);
//}

// --- コライダーコールバック ---

void ScTutorial::OnCollisionEnter(SeedCore::Entity entity)
{
	// entity は衝突してきた相手のEntity。GetWorld().GetActor(entity)で相手のActorが取れる
	//SeedCore::Actor otherActor = GetWorld().GetActor(entity);

	// 相手が何なのかを判定したいときは、名前で分岐するよりタグ/レイヤーで判定するのがおすすめ
	// タグ=1つのActorに複数付けられる、ゆるいカテゴリ分け用（"Enemy"かつ"Flying"のように重ねられる）
	// レイヤー=1つのActorに1つだけ、主に物理の衝突フィルタ（LayerSettingsPanelの衝突マトリクス）用
	// どちらもインスペクターで設定する
	//if (otherActor.HasTag(SeedCore::String("Enemy")))
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

//=== Prefab ===//

// Prefabを実行中に出したいときはPrefabクラスの静的関数を使う。
// Scene/Prefabの static 関数は、プロセス全体のWorld/ResourceCacheを(Editor.cpp/Runtime側で)束縛済みなので、
// SeedScriptのOnTick等からでも、Worldを一切意識せず呼べる
//
// ファイル名だけで呼べる（内部でResourceCacheのアセット名検索に通してから開く）
//SeedCore::Actor bullet = SeedCore::Prefab::Spawn(SeedCore::String("Bullet.prefab"));
//
// 第2引数に親Actorを渡すと、その子として出る
//SeedCore::Actor effect = SeedCore::Prefab::Spawn(SeedCore::String("Muzzle.prefab"), GetActor());
//
// SC_PAYLOAD_FIELDでインスペクターから設定したアセットIDを、そのまま渡すこともできる
//SeedCore::Actor spawned = SeedCore::Prefab::Spawn(demo7_);
//
// 出したものを時間で消したいときは、自分で数えるよりLifetimeコンポーネントを付けるのが楽
// （インスペクターの「生存時間(秒)」を過ぎたら自動で破棄される）

//=== Audio ===//

// 音はAudioSourceコンポーネントに.audioアセットを設定して、キュー名で鳴らす。
// AudioSource自体もSeedScriptなので、他のコンポーネントと同じようにWorldから取れる
//SeedCore::AudioSource* audio = GetWorld().GetComponent<SeedCore::AudioSource>(GetActor().GetEntity());
//if (audio)
//{
//	audio->Play(SeedCore::String("Shot"));   // インスペクターで選んだキューとは別のキューを鳴らす場合
//	audio->Play();                            // インスペクターで選んだキューをそのまま鳴らす場合
//}

//=== Scene ===//

// シーンを切り替えたいときはSceneクラスの静的関数を使う。こちらもファイル名だけで呼べる
//
// 即座に切り替える場合
//SeedCore::Scene::Change("NextScene.scene");
//
// フェードアウト/フェードインしながら非同期に切り替える場合
//SeedCore::Scene::Change("NextScene.scene", 0.3f, 0.3f);
//
// ローディングシーンを挟んで非同期に切り替える場合
//SeedCore::Scene::Change("NextScene.scene", "LoadingScene.scene");
//
// Scene::Update(deltaTime)は遷移の状態機械を進める処理で、こちらは呼ぶ必要はない
// （Editorと Runtime の両方で毎フレーム呼ばれている）
//
// パスからAssetIDを引きたいとき（PrefabやTexture等、他の何かにアセットIDを渡す前段として）はScene::AssetIDを使う
// これもSceneと同じ束縛済みのResourceCache経由で解決するので、SeedScriptから直接呼べる。こちらもファイル名だけで良い
//SeedCore::Uint32 targetSceneAssetID = SeedCore::Scene::AssetID(SeedCore::String("NextScene.scene"));
