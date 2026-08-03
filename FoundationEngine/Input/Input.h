#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	/**
	* [EN]
	* Static, process-wide backend that polls raw keyboard (Win32),
	* mouse (Win32), and gamepad (SDL) state once per frame via Update().
	* Exposes edge-triggered queries (rising/falling) built from the
	* current vs. previous frame's snapshot. InputSystem is the stable
	* public-facing facade over this class; gameplay code should prefer
	* InputSystem.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 生のキーボード（Win32）、マウス（Win32）、ゲームパッド（SDL）状態を
	* Update() で毎フレーム1回ポーリングする、静的でプロセス全体の
	* バックエンド。現在フレームと前フレームのスナップショットから、
	* エッジトリガー（押した/離した瞬間）のクエリを構築して公開する。
	* InputSystem はこのクラスの安定した公開向けファサードであり、
	* ゲームプレイコードは InputSystem を使うべき。
	*/
	class Input
	{
	public:
		/// [EN] Re-exported for convenience so callers don't need to name InputSystem directly.
		/// [JP] 呼び出し側が InputSystem を直接名指ししなくて済むよう、利便性のため再エクスポートする。
		using TriggerMode = InputSystem::TriggerMode;

		/// [EN] Re-exported for convenience so callers don't need to name InputSystem directly.
		/// [JP] 呼び出し側が InputSystem を直接名指ししなくて済むよう、利便性のため再エクスポートする。
		using Key = InputSystem::Key;

		/// [EN] Re-exported for convenience so callers don't need to name InputSystem directly.
		/// [JP] 呼び出し側が InputSystem を直接名指ししなくて済むよう、利便性のため再エクスポートする。
		using MouseButton = InputSystem::MouseButton;

		/// [EN] Re-exported for convenience so callers don't need to name InputSystem directly.
		/// [JP] 呼び出し側が InputSystem を直接名指ししなくて済むよう、利便性のため再エクスポートする。
		using StickSide = InputSystem::StickSide;

		/// [EN] Re-exported for convenience so callers don't need to name InputSystem directly.
		/// [JP] 呼び出し側が InputSystem を直接名指ししなくて済むよう、利便性のため再エクスポートする。
		using DirectionalKeys = InputSystem::DirectionalKeys;

		/// [EN] Number of tracked virtual-key slots.
		/// [JP] 追跡する仮想キースロットの数。
		static constexpr Int KEY_COUNT = 256;

		/// [EN] Number of tracked mouse buttons.
		/// [JP] 追跡するマウスボタンの数。
		static constexpr Int MOUSE_BUTTON_COUNT = 5;

		/// [EN] Number of tracked gamepad buttons.
		/// [JP] 追跡するゲームパッドボタンの数。
		static constexpr Int GAMEPAD_BUTTON_COUNT = SDL_GAMEPAD_BUTTON_COUNT;

		/**
		* [EN]
		* Initializes the SDL gamepad subsystem and loads the action binding
		* table (see LoadBindings()). Must be called once before any other
		* Input method.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* SDLのゲームパッドサブシステムを初期化し、アクションバインド
		* テーブルを読み込む（LoadBindings() 参照）。他のどの Input
		* メソッドよりも先に、一度だけ呼び出す必要がある。
		*/
		static void Initialize();

		/**
		* [EN]
		* Polls keyboard/mouse/gamepad state for the current frame,
		* rotating the previous-frame snapshot forward. Call once per frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在フレームのキーボード/マウス/ゲームパッド状態をポーリングし、
		* 前フレームのスナップショットを繰り越す。毎フレーム1回呼び出す。
		*/
		static void Update();

		/**
		* [EN]
		* Closes the active gamepad (if any) and shuts down the SDL
		* gamepad subsystem.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アクティブなゲームパッド（あれば）を閉じ、SDLのゲームパッド
		* サブシステムを終了する。
		*/
		static void Finalize();

		/**
		* [EN]
		* Enables or disables KeyState()/MouseState()/MouseDeltaX()/
		* MouseDeltaY()/MouseWheelDelta() queries. While disabled, all
		* report nothing pressed / zero movement (edge queries report no
		* transition).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* KeyState()/MouseState()/MouseDeltaX()/MouseDeltaY()/
		* MouseWheelDelta() の問い合わせを有効/無効にする。無効の間は
		* いずれも「何も押されていない/移動量ゼロ」を返す（エッジクエリは
		* 遷移なしを返す）。
		*/
		static void SetInputEnabled(Bool enabled);

		/**
		* [EN]
		* Returns whether input queries are currently enabled (see SetInputEnabled()).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 入力の問い合わせが現在有効かどうかを返す（SetInputEnabled() 参照）。
		*/
		static Bool IsInputEnabled();

		/**
		* [EN]
		* Returns whether the virtual key vkey satisfies mode (current
		* state, or a rising/falling edge versus last frame). Returns
		* false if vkey is out of range.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 仮想キー vkey が mode を満たすか（現在の状態、または前フレームとの
		* 立ち上がり/立ち下がりエッジ）を返す。vkey が範囲外なら false。
		*/
		static Bool KeyState(Key key, TriggerMode mode);

		/**
		* [EN]
		* Returns whether mouse button button satisfies mode. Returns
		* false if button is out of range.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マウスボタン button が mode を満たすかを返す。button が範囲外なら false。
		*/
		static Bool MouseState(MouseButton button, TriggerMode mode);

		/**
		* [EN]
		* Returns the cursor's current X position, in screen pixels.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* カーソルの現在のX座標を、画面ピクセル単位で返す。
		*/
		static Float MouseX();

		/**
		* [EN]
		* Returns the cursor's current Y position, in screen pixels.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* カーソルの現在のY座標を、画面ピクセル単位で返す。
		*/
		static Float MouseY();

		/**
		* [EN]
		* Returns the cursor's X movement since last frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前フレームからのカーソルのX方向の移動量を返す。
		*/
		static Float MouseDeltaX();

		/**
		* [EN]
		* Returns the cursor's Y movement since last frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前フレームからのカーソルのY方向の移動量を返す。
		*/
		static Float MouseDeltaY();

		/**
		* [EN]
		* Returns the mouse wheel delta accumulated last frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前フレームに累積されたマウスホイールのデルタを返す。
		*/
		static Float MouseWheelDelta();

		/**
		* [EN]
		* Accumulates delta into the current frame's mouse wheel total,
		* to be read back via MouseWheelDelta() next Update(). Called from
		* the platform message loop (SDL doesn't surface wheel events here).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* delta を現在フレームのマウスホイール合計に累積する。次の
		* Update() 後に MouseWheelDelta() で読み出される。プラットフォームの
		* メッセージループから呼ばれる（SDLはここではホイールイベントを
		* 提供しないため）。
		*/
		static void PushMouseWheel(Float delta);

		/**
		* [EN]
		* Begins re-anchoring the cursor each frame (see Update()) so
		* camera-look style dragging never hits a monitor edge. No-op if
		* already captured.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 毎フレームカーソルを再アンカーする（Update() を参照）ことを
		* 開始し、カメラ視点ドラッグ操作がモニタ端に到達しないようにする。
		* 既にキャプチャ中なら何もしない。
		*/
		static void BeginMouseCapture();

		/**
		* [EN]
		* Ends mouse capture, restoring the cursor to its position when
		* BeginMouseCapture() was called. No-op if not captured.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マウスキャプチャを終了し、カーソルを BeginMouseCapture() が
		* 呼ばれた時点の位置へ戻す。キャプチャ中でなければ何もしない。
		*/
		static void EndMouseCapture();

		/**
		* [EN]
		* Returns whether mouse capture is currently active.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マウスキャプチャが現在アクティブかどうかを返す。
		*/
		static Bool IsMouseCaptured();

		/**
		* [EN]
		* Returns whether gamepad button button satisfies mode. Returns
		* false if button is out of range.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ゲームパッドボタン button が mode を満たすかを返す。button が
		* 範囲外なら false。
		*/
		static Bool GamepadState(SDL_GamepadButton button, TriggerMode mode);

		/**
		* [EN]
		* Returns the left stick's X axis, normalized to [-1, 1].
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 左スティックのX軸を [-1, 1] に正規化して返す。
		*/
		static Float GamepadAxisLX();

		/**
		* [EN]
		* Returns the left stick's Y axis, normalized to [-1, 1].
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 左スティックのY軸を [-1, 1] に正規化して返す。
		*/
		static Float GamepadAxisLY();

		/**
		* [EN]
		* Returns the right stick's X axis, normalized to [-1, 1].
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 右スティックのX軸を [-1, 1] に正規化して返す。
		*/
		static Float GamepadAxisRX();

		/**
		* [EN]
		* Returns the right stick's Y axis, normalized to [-1, 1].
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 右スティックのY軸を [-1, 1] に正規化して返す。
		*/
		static Float GamepadAxisRY();

		/**
		* [EN]
		* Rumbles the gamepad's low/high frequency motors for durationMs
		* milliseconds. No-op if no gamepad is connected.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ゲームパッドの低周波/高周波モーターを durationMs ミリ秒間振動
		* させる。ゲームパッドが接続されていなければ何もしない。
		*/
		static void Rumble(Uint16 lowFrequency, Uint16 highFrequency, Uint32 durationMs);

		/**
		* [EN]
		* Rumbles the gamepad's trigger motors (if supported) for
		* durationMs milliseconds. No-op if no gamepad is connected.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ゲームパッドのトリガーモーター（対応していれば）を durationMs
		* ミリ秒間振動させる。ゲームパッドが接続されていなければ何もしない。
		*/
		static void RumbleTriggers(Uint16 left, Uint16 right, Uint32 durationMs);

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
		static Bool ActionState(String action, TriggerMode mode);

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
		static void RegisterAction(String action);

		/**
		* [EN]
		* Adds key to action's key bindings. No-op if already bound.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* action のキーバインドに key を追加する。既に紐づいていれば何もしない。
		*/
		static void BindKey(String action, Key key);

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
		static void BindGamepadButton(String action, SDL_GamepadButton button);

		/**
		* [EN]
		* Removes key from action's key bindings, if present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* action のキーバインドから key を取り除く（存在すれば）。
		*/
		static void UnbindKey(String action, Key key);

		/**
		* [EN]
		* Removes button from action's gamepad-button bindings, if present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* action のゲームパッドボタンバインドから button を取り除く（存在すれば）。
		*/
		static void UnbindGamepadButton(String action, SDL_GamepadButton button);

		/**
		* [EN]
		* Removes action entirely (all its key/gamepad bindings).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* action を（キー/ゲームパッドの全バインドごと）完全に削除する。
		*/
		static void RemoveAction(String action);

		/**
		* [EN]
		* Returns the keys currently bound to action. Empty if action is unknown.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* action に現在紐づいているキーの一覧を返す。action が未知なら空。
		*/
		static const DynamicArray<Key>& GetBoundKeys(String action);

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
		static const DynamicArray<SDL_GamepadButton>& GetBoundGamepadButtons(String action);

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
		static void BindAxisKeys(String action, DirectionalKeys keys);

		/**
		* [EN]
		* Removes keys from action's axis bindings, if an exact match is present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* action の軸バインドから keys を取り除く（完全一致するものがあれば）。
		*/
		static void UnbindAxisKeys(String action, DirectionalKeys keys);

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
		static void BindStick(String action, StickSide side);

		/**
		* [EN]
		* Removes side from action's bound analog sticks, if present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* action の紐づくアナログスティックから side を取り除く（存在すれば）。
		*/
		static void UnbindStick(String action, StickSide side);

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
		static Vector2 ActionAxis2D(String action);

		/**
		* [EN]
		* Returns the directional-key composites currently bound to action. Empty if action is unknown.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* action に現在紐づいている方向キー組の一覧を返す。action が未知なら空。
		*/
		static const DynamicArray<DirectionalKeys>& GetBoundAxisKeys(String action);

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
		static const DynamicArray<StickSide>& GetBoundSticks(String action);

		/**
		* [EN]
		* Returns every known action name, in the order each was first bound.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 既知の全アクション名を、最初に紐づけられた順で返す。
		*/
		static const DynamicArray<String>& GetActionNames();

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
		static void LoadBindings(const std::filesystem::path& path = "../UserProject/Assets/Config/InputBindings.scg");

		/**
		* [EN]
		* Saves the current action binding table to path (JSON).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在のアクションバインドテーブルを path（JSON）へ保存する。
		*/
		static void SaveBindings(const std::filesystem::path& path = "../UserProject/Assets/Config/InputBindings.scg");

	private:
		Input() = delete;

		/**
		* [EN]
		* Normalizes a raw SDL stick axis value (Sint16 range) to [-1, 1].
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 生のSDLスティック軸の値（Sint16 の範囲）を [-1, 1] に正規化する。
		*/
		static Float NormalizeAxis(Sint16 value);

		/// [EN] Gate for KeyState()/MouseState(); see SetInputEnabled().
		/// [JP] KeyState()/MouseState() 用のゲート。SetInputEnabled() を参照。
		static Bool inputEnabled_;

		/// [EN] This frame's key-down state, indexed by virtual-key code.
		/// [JP] 現在フレームのキー押下状態。仮想キーコードでインデックスする。
		static Bool currentKeys_[KEY_COUNT];

		/// [EN] Last frame's key-down state, used to detect edges.
		/// [JP] 前フレームのキー押下状態。エッジ検出に使う。
		static Bool previousKeys_[KEY_COUNT];

		/// [EN] This frame's mouse button state.
		/// [JP] 現在フレームのマウスボタン状態。
		static Bool currentMouse_[MOUSE_BUTTON_COUNT];

		/// [EN] Last frame's mouse button state, used to detect edges.
		/// [JP] 前フレームのマウスボタン状態。エッジ検出に使う。
		static Bool previousMouse_[MOUSE_BUTTON_COUNT];

		/// [EN] Cursor's current X position, in screen pixels.
		/// [JP] カーソルの現在のX座標（画面ピクセル単位）。
		static Float mouseX_;

		/// [EN] Cursor's current Y position, in screen pixels.
		/// [JP] カーソルの現在のY座標（画面ピクセル単位）。
		static Float mouseY_;

		/// [EN] Cursor's X movement since last frame.
		/// [JP] 前フレームからのカーソルのX方向の移動量。
		static Float mouseDeltaX_;

		/// [EN] Cursor's Y movement since last frame.
		/// [JP] 前フレームからのカーソルのY方向の移動量。
		static Float mouseDeltaY_;

		/// [EN] Mouse wheel delta accumulated since the last Update().
		/// [JP] 直近の Update() 以降に累積されたマウスホイールのデルタ。
		static Float mouseWheelDelta_;

		/// [EN] Mouse wheel delta accumulated during the last completed frame (read by MouseWheelDelta()).
		/// [JP] 直近に完了したフレーム中に累積されたマウスホイールのデルタ（MouseWheelDelta() が返す値）。
		static Float mouseWheelDeltaFrame_;

		/// [EN] While the right mouse button is held (camera-look drag), the
		///      cursor is warped back to mouseCaptureAnchor every frame so it
		///      can never reach a monitor edge (see Input::Update). mouseCaptureReturn
		///      is where the cursor was when the drag started, restored on release.
		/// [JP] 右クリック押下中（カメラ視点ドラッグ）は、カーソルを毎フレーム
		///      mouseCaptureAnchor へ戻すことでモニタ端に到達しないようにする
		///      (Input::Update 参照)。mouseCaptureReturn はドラッグ開始時の
		///      カーソル位置で、離した時にそこへ復帰する。
		static Bool mouseCaptured_;
		static Float mouseCaptureAnchorX_;
		static Float mouseCaptureAnchorY_;
		static Float mouseCaptureReturnX_;
		static Float mouseCaptureReturnY_;

		/// [EN] This frame's gamepad button state.
		/// [JP] 現在フレームのゲームパッドボタン状態。
		static Bool currentGamepad_[GAMEPAD_BUTTON_COUNT];

		/// [EN] Last frame's gamepad button state, used to detect edges.
		/// [JP] 前フレームのゲームパッドボタン状態。エッジ検出に使う。
		static Bool previousGamepad_[GAMEPAD_BUTTON_COUNT];

		/// [EN] Left stick X axis, normalized to [-1, 1].
		/// [JP] 左スティックのX軸（[-1, 1] に正規化）。
		static Float axisLX_;

		/// [EN] Left stick Y axis, normalized to [-1, 1].
		/// [JP] 左スティックのY軸（[-1, 1] に正規化）。
		static Float axisLY_;

		/// [EN] Right stick X axis, normalized to [-1, 1].
		/// [JP] 右スティックのX軸（[-1, 1] に正規化）。
		static Float axisRX_;

		/// [EN] Right stick Y axis, normalized to [-1, 1].
		/// [JP] 右スティックのY軸（[-1, 1] に正規化）。
		static Float axisRY_;

		/// [EN] Handle to the first connected gamepad, or nullptr if none.
		/// [JP] 最初に接続されたゲームパッドへのハンドル。なければ nullptr。
		static SDL_Gamepad* gamepad_;

		/// [EN] One action's bound keys and gamepad buttons.
		/// [JP] 1つのアクションに紐づくキーとゲームパッドボタン。
		struct ActionBinding
		{
			/// [EN] Keys bound to this action.
			/// [JP] このアクションに紐づくキー。
			DynamicArray<Key> keys_;

			/// [EN] Gamepad buttons bound to this action.
			/// [JP] このアクションに紐づくゲームパッドボタン。
			DynamicArray<SDL_GamepadButton> gamepadButtons_;

			/// [EN] Directional-key composites bound to this action (see ActionAxis2D()).
			/// [JP] このアクションに紐づく方向キー組（ActionAxis2D() 参照）。
			DynamicArray<DirectionalKeys> axisKeys_;

			/// [EN] Analog sticks bound to this action (see ActionAxis2D()).
			/// [JP] このアクションに紐づくアナログスティック（ActionAxis2D() 参照）。
			DynamicArray<StickSide> sticks_;
		};

		/**
		* [EN]
		* Returns actionBindings_'s entry for action, creating an empty
		* one (and appending to actionOrder_) if action is new.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* action に対応する actionBindings_ のエントリを返す。action が
		* 未知なら空のエントリを作成し（actionOrder_ にも追加し）返す。
		*/
		static ActionBinding& GetOrCreateActionBinding(const String& action);

		/// [EN] Action name -> its bound keys/gamepad buttons.
		/// [JP] アクション名 -> 紐づくキー/ゲームパッドボタン。
		static inline FlatMap<String, ActionBinding> actionBindings_;

		/// [EN] Action names in first-bound order, for stable enumeration (see GetActionNames()).
		/// [JP] 最初に紐づけられた順のアクション名。安定した列挙のため（GetActionNames() 参照）。
		static inline DynamicArray<String> actionOrder_;
	};
}
