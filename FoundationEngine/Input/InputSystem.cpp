#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/Input/Input.h>

namespace SeedCore
{
	/**
	* [EN]
	* Initializes the input backend. Must be called once before any
	* other InputSystem method.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 入力バックエンドを初期化する。他のどの InputSystem メソッドよりも
	* 先に、一度だけ呼び出す必要がある。
	*/
	void InputSystem::Initialize()
	{
		Input::Initialize();
	}

	/**
	* [EN]
	* Polls input state for the current frame. Call once per frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在フレームの入力状態をポーリングする。毎フレーム1回呼び出す。
	*/
	void InputSystem::Update()
	{
		Input::Update();
	}

	/**
	* [EN]
	* Shuts down the input backend.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 入力バックエンドを終了する。
	*/
	void InputSystem::Finalize()
	{
		Input::Finalize();
	}

	/**
	* [EN]
	* Enables or disables KeyState()/MouseState()/MouseDeltaX()/
	* MouseDeltaY()/MouseWheelDelta() queries. While disabled, all
	* report nothing pressed / zero movement (edge queries report no
	* transition). Pure on/off mechanism, same shape as
	* BeginMouseCapture/EndMouseCapture — deciding when to flip it is
	* the caller's job.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* KeyState()/MouseState()/MouseDeltaX()/MouseDeltaY()/
	* MouseWheelDelta() の問い合わせを有効/無効にする。無効の間は
	* いずれも「何も押されていない/移動量ゼロ」を返す（エッジクエリは
	* 遷移なしを返す）。BeginMouseCapture/EndMouseCapture と同じ形の、
	* 単純なオン/オフ機構であり、いつ切り替えるかは呼び出し側の判断。
	*/
	void InputSystem::SetInputEnabled(Bool enabled)
	{
		Input::SetInputEnabled(enabled);
	}

	/**
	* [EN]
	* Returns whether input queries are currently enabled (see SetInputEnabled()).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 入力の問い合わせが現在有効かどうかを返す（SetInputEnabled() 参照）。
	*/
	Bool InputSystem::IsInputEnabled()
	{
		return Input::IsInputEnabled();
	}

	/**
	* [EN]
	* Returns whether the virtual key vkey satisfies mode.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 仮想キー vkey が mode を満たすかを返す。
	*/
	Bool InputSystem::KeyState(Key key, TriggerMode mode)
	{
		return Input::KeyState(key, mode);
	}

	/**
	* [EN]
	* Returns whether mouse button button satisfies mode.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* マウスボタン button が mode を満たすかを返す。
	*/
	Bool InputSystem::MouseState(MouseButton button, TriggerMode mode)
	{
		return Input::MouseState(button, mode);
	}

	/**
	* [EN]
	* Returns the cursor's current X position, in screen pixels.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* カーソルの現在のX座標を、画面ピクセル単位で返す。
	*/
	Float InputSystem::MouseX()
	{
		return Input::MouseX();
	}

	/**
	* [EN]
	* Returns the cursor's current Y position, in screen pixels.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* カーソルの現在のY座標を、画面ピクセル単位で返す。
	*/
	Float InputSystem::MouseY()
	{
		return Input::MouseY();
	}

	/**
	* [EN]
	* Returns the cursor's X movement since last frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 前フレームからのカーソルのX方向の移動量を返す。
	*/
	Float InputSystem::MouseDeltaX()
	{
		return Input::MouseDeltaX();
	}

	/**
	* [EN]
	* Returns the cursor's Y movement since last frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 前フレームからのカーソルのY方向の移動量を返す。
	*/
	Float InputSystem::MouseDeltaY()
	{
		return Input::MouseDeltaY();
	}

	/**
	* [EN]
	* Returns the mouse wheel delta accumulated last frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 前フレームに累積されたマウスホイールのデルタを返す。
	*/
	Float InputSystem::MouseWheelDelta()
	{
		return Input::MouseWheelDelta();
	}

	/**
	* [EN]
	* Accumulates delta into the current frame's mouse wheel total.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* delta を現在フレームのマウスホイール合計に累積する。
	*/
	void InputSystem::PushMouseWheel(Float delta)
	{
		Input::PushMouseWheel(delta);
	}

	/**
	* [EN]
	* Begins re-anchoring the cursor each frame so camera-look style
	* dragging never hits a monitor edge.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 毎フレームカーソルを再アンカーすることを開始し、カメラ視点
	* ドラッグ操作がモニタ端に到達しないようにする。
	*/
	void InputSystem::BeginMouseCapture()
	{
		Input::BeginMouseCapture();
	}

	/**
	* [EN]
	* Ends mouse capture, restoring the cursor to its position when
	* BeginMouseCapture() was called.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* マウスキャプチャを終了し、カーソルを BeginMouseCapture() が
	* 呼ばれた時点の位置へ戻す。
	*/
	void InputSystem::EndMouseCapture()
	{
		Input::EndMouseCapture();
	}

	/**
	* [EN]
	* Returns whether mouse capture is currently active.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* マウスキャプチャが現在アクティブかどうかを返す。
	*/
	Bool InputSystem::IsMouseCaptured()
	{
		return Input::IsMouseCaptured();
	}

	/**
	* [EN]
	* Returns whether gamepad button button satisfies mode.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ゲームパッドボタン button が mode を満たすかを返す。
	*/
	Bool InputSystem::GamepadState(SDL_GamepadButton button, TriggerMode mode)
	{
		return Input::GamepadState(button, mode);
	}

	/**
	* [EN]
	* Returns the left stick's X axis, normalized to [-1, 1].
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 左スティックのX軸を [-1, 1] に正規化して返す。
	*/
	Float InputSystem::GamepadAxisLX()
	{
		return Input::GamepadAxisLX();
	}

	/**
	* [EN]
	* Returns the left stick's Y axis, normalized to [-1, 1].
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 左スティックのY軸を [-1, 1] に正規化して返す。
	*/
	Float InputSystem::GamepadAxisLY()
	{
		return Input::GamepadAxisLY();
	}

	/**
	* [EN]
	* Returns the right stick's X axis, normalized to [-1, 1].
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 右スティックのX軸を [-1, 1] に正規化して返す。
	*/
	Float InputSystem::GamepadAxisRX()
	{
		return Input::GamepadAxisRX();
	}

	/**
	* [EN]
	* Returns the right stick's Y axis, normalized to [-1, 1].
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 右スティックのY軸を [-1, 1] に正規化して返す。
	*/
	Float InputSystem::GamepadAxisRY()
	{
		return Input::GamepadAxisRY();
	}

	/**
	* [EN]
	* Rumbles the gamepad's low/high frequency motors for durationMs milliseconds.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ゲームパッドの低周波/高周波モーターを durationMs ミリ秒間振動させる。
	*/
	void InputSystem::Rumble(Uint16 lowFrequency, Uint16 highFrequency, Uint32 durationMs)
	{
		Input::Rumble(lowFrequency, highFrequency, durationMs);
	}

	/**
	* [EN]
	* Rumbles the gamepad's trigger motors (if supported) for durationMs milliseconds.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ゲームパッドのトリガーモーター（対応していれば）を durationMs
	* ミリ秒間振動させる。
	*/
	void InputSystem::RumbleTriggers(Uint16 left, Uint16 right, Uint32 durationMs)
	{
		Input::RumbleTriggers(left, right, durationMs);
	}

	/**
	* [EN]
	* Returns whether action is currently bound to key (KeyState) or
	* button (GamepadState) satisfies mode, so callers don't have to
	* branch on which device the player is using.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action に紐づく、いずれかのキー（KeyState）またはゲームパッド
	* ボタン（GamepadState）が mode を満たすかを返す。呼び出し側が
	* プレイヤーの使用デバイスで分岐せずに済む。
	*/
	Bool InputSystem::ActionState(String action, TriggerMode mode)
	{
		return Input::ActionState(action, mode);
	}

	/**
	* [EN]
	* Registers action with no bindings yet, if it doesn't already
	* exist. No-op if it does.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action をバインドなしで登録する（まだ存在しなければ）。既に
	* 存在していれば何もしない。
	*/
	void InputSystem::RegisterAction(String action)
	{
		Input::RegisterAction(action);
	}

	/**
	* [EN]
	* Adds key to action's key bindings. No-op if already bound.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action のキーバインドに key を追加する。既に紐づいていれば何もしない。
	*/
	void InputSystem::BindKey(String action, Key key)
	{
		Input::BindKey(action, key);
	}

	/**
	* [EN]
	* Adds button to action's gamepad-button bindings. No-op if already bound.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action のゲームパッドボタンバインドに button を追加する。既に
	* 紐づいていれば何もしない。
	*/
	void InputSystem::BindGamepadButton(String action, SDL_GamepadButton button)
	{
		Input::BindGamepadButton(action, button);
	}

	/**
	* [EN]
	* Removes key from action's key bindings, if present.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action のキーバインドから key を取り除く（存在すれば）。
	*/
	void InputSystem::UnbindKey(String action, Key key)
	{
		Input::UnbindKey(action, key);
	}

	/**
	* [EN]
	* Removes button from action's gamepad-button bindings, if present.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action のゲームパッドボタンバインドから button を取り除く（存在すれば）。
	*/
	void InputSystem::UnbindGamepadButton(String action, SDL_GamepadButton button)
	{
		Input::UnbindGamepadButton(action, button);
	}

	/**
	* [EN]
	* Removes action entirely (all its key/gamepad bindings).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action を（キー/ゲームパッドの全バインドごと）完全に削除する。
	*/
	void InputSystem::RemoveAction(String action)
	{
		Input::RemoveAction(action);
	}

	/**
	* [EN]
	* Returns the keys currently bound to action. Empty if action is unknown.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action に現在紐づいているキーの一覧を返す。action が未知なら空。
	*/
	const DynamicArray<InputSystem::Key>& InputSystem::GetBoundKeys(String action)
	{
		return Input::GetBoundKeys(action);
	}

	/**
	* [EN]
	* Returns the gamepad buttons currently bound to action. Empty if action is unknown.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action に現在紐づいているゲームパッドボタンの一覧を返す。action が
	* 未知なら空。
	*/
	const DynamicArray<SDL_GamepadButton>& InputSystem::GetBoundGamepadButtons(String action)
	{
		return Input::GetBoundGamepadButtons(action);
	}

	/**
	* [EN]
	* Adds one WASD/arrow-key-style directional composite (up/down/
	* left/right) to action's axis bindings. No-op if the exact same
	* four keys are already bound.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action の軸バインドに、WASD/矢印キー的な方向キー1組（上下左右）を
	* 追加する。全く同じ4キーが既に紐づいていれば何もしない。
	*/
	void InputSystem::BindAxisKeys(String action, DirectionalKeys keys)
	{
		Input::BindAxisKeys(action, keys);
	}

	/**
	* [EN]
	* Removes keys from action's axis bindings, if an exact match is present.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action の軸バインドから keys を取り除く（完全一致するものがあれば）。
	*/
	void InputSystem::UnbindAxisKeys(String action, DirectionalKeys keys)
	{
		Input::UnbindAxisKeys(action, keys);
	}

	/**
	* [EN]
	* Adds side to action's bound analog sticks. No-op if already bound.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action の紐づくアナログスティックに side を追加する。既に
	* 紐づいていれば何もしない。
	*/
	void InputSystem::BindStick(String action, StickSide side)
	{
		Input::BindStick(action, side);
	}

	/**
	* [EN]
	* Removes side from action's bound analog sticks, if present.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action の紐づくアナログスティックから side を取り除く（存在すれば）。
	*/
	void InputSystem::UnbindStick(String action, StickSide side)
	{
		Input::UnbindStick(action, side);
	}

	/**
	* [EN]
	* Returns action's combined 2D input as a Vector2: whichever of its
	* bound directional-key composites are currently held, normalized;
	* if none are held, the first bound stick reporting nonzero input;
	* otherwise (0, 0). Lets movement code read one action regardless
	* of whether the player is on keyboard or gamepad.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action の入力を合成した Vector2 を返す: 紐づく方向キー組のうち
	* 現在押されているものを正規化して返す。どれも押されていなければ、
	* 紐づくスティックのうち最初に非ゼロを報告したものを返す。
	* どちらも無ければ (0, 0)。移動処理側はプレイヤーがキーボードか
	* ゲームパッドかを問わず、1つのアクションを読むだけで済む。
	*/
	Vector2 InputSystem::ActionAxis2D(String action)
	{
		return Input::ActionAxis2D(action);
	}

	/**
	* [EN]
	* Returns the directional-key composites currently bound to action. Empty if action is unknown.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action に現在紐づいている方向キー組の一覧を返す。action が未知なら空。
	*/
	const DynamicArray<InputSystem::DirectionalKeys>& InputSystem::GetBoundAxisKeys(String action)
	{
		return Input::GetBoundAxisKeys(action);
	}

	/**
	* [EN]
	* Returns the analog sticks currently bound to action. Empty if action is unknown.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action に現在紐づいているアナログスティックの一覧を返す。action が
	* 未知なら空。
	*/
	const DynamicArray<InputSystem::StickSide>& InputSystem::GetBoundSticks(String action)
	{
		return Input::GetBoundSticks(action);
	}

	/**
	* [EN]
	* Returns every known action name, in the order each was first bound.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 既知の全アクション名を、最初に紐づけられた順で返す。
	*/
	const DynamicArray<String>& InputSystem::GetActionNames()
	{
		return Input::GetActionNames();
	}

	/**
	* [EN]
	* Loads action bindings from path (JSON), replacing the current
	* table. Missing file or unparseable fields are silently ignored.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* path（JSON）からアクションバインドを読み込み、現在のテーブルを
	* 置き換える。ファイルが無い、またはフィールドが解釈できない場合は
	* 何もせず無視する。
	*/
	void InputSystem::LoadBindings(const std::filesystem::path& path)
	{
		Input::LoadBindings(path);
	}

	/**
	* [EN]
	* Saves the current action binding table to path (JSON).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在のアクションバインドテーブルを path（JSON）へ保存する。
	*/
	void InputSystem::SaveBindings(const std::filesystem::path& path)
	{
		Input::SaveBindings(path);
	}
}
