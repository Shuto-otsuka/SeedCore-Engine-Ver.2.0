#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Stable, public-facing input API for gameplay/editor code. Every
	* method simply forwards to the matching Input method (see
	* FoundationEngine/Input/Input.h); this indirection keeps game-facing
	* call sites decoupled from Input's Win32/SDL-specific implementation.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ゲームプレイ/エディタコード向けの、安定した公開入力API。各メソッドは
	* 対応する Input のメソッド（FoundationEngine/Input/Input.h 参照）へ
	* 単純に転送する。この間接層により、ゲーム向けの呼び出し箇所を
	* Input の Win32/SDL 依存の実装から切り離す。
	*/
	class SEEDCORE_API InputSystem
	{
	public:
		/// [EN] Edge-triggering mode for state queries (KeyState/MouseState/GamepadState).
		/// [JP] 状態クエリ（KeyState/MouseState/GamepadState）のエッジトリガーモード。
		enum class TriggerMode
		{
			/// [EN] Query the current held state, regardless of last frame.
			/// [JP] 前フレームに関わらず、現在の押下状態を問い合わせる。
			NONE,

			/// [EN] True only on the frame the input transitions from up to down.
			/// [JP] 入力が離れた状態から押された状態へ遷移したフレームのみ true。
			RISING_EDGE,

			/// [EN] True only on the frame the input transitions from down to up.
			/// [JP] 入力が押された状態から離れた状態へ遷移したフレームのみ true。
			FALLING_EDGE
		};

		/// [EN] Readable keyboard key codes for use at KeyState() call sites, in place of raw Win32 virtual-key codes.
		/// [JP] KeyState() の呼び出し箇所で、生のWin32仮想キーコードの代わりに使う、読みやすいキーボードキーコード。
		enum class Key : Int
		{
			Backspace = VK_BACK,
			Tab = VK_TAB,
			Enter = VK_RETURN,
			Shift = VK_SHIFT,
			Control = VK_CONTROL,
			Alt = VK_MENU,
			CapsLock = VK_CAPITAL,
			Escape = VK_ESCAPE,
			Space = VK_SPACE,
			PageUp = VK_PRIOR,
			PageDown = VK_NEXT,
			Home = VK_HOME,
			End = VK_END,
			Left = VK_LEFT,
			Up = VK_UP,
			Right = VK_RIGHT,
			Down = VK_DOWN,
			Delete = VK_DELETE,

			LeftShift = VK_LSHIFT,
			RightShift = VK_RSHIFT,
			LeftControl = VK_LCONTROL,
			RightControl = VK_RCONTROL,
			LeftAlt = VK_LMENU,
			RightAlt = VK_RMENU,

			Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4',
			Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9',

			A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G',
			H = 'H', I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N',
			O = 'O', P = 'P', Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U',
			V = 'V', W = 'W', X = 'X', Y = 'Y', Z = 'Z',

			F1 = VK_F1, F2 = VK_F2, F3 = VK_F3, F4 = VK_F4,
			F5 = VK_F5, F6 = VK_F6, F7 = VK_F7, F8 = VK_F8,
			F9 = VK_F9, F10 = VK_F10, F11 = VK_F11, F12 = VK_F12
		};

		/// [EN] Readable mouse button codes for use at MouseState() call sites, in place of raw button indices.
		/// [JP] MouseState() の呼び出し箇所で、生のボタン番号の代わりに使う、読みやすいマウスボタンコード。
		enum class MouseButton : Int
		{
			Left,
			Right,
			Middle,
			Extra1,
			Extra2
		};

		/// [EN] Readable aliases for GamepadState() call sites, in place of raw SDL_GamepadButton values.
		/// [JP] GamepadState() の呼び出し箇所で、生の SDL_GamepadButton 値の代わりに使う、読みやすいエイリアス。
		struct GamepadButton
		{
			static constexpr SDL_GamepadButton A = SDL_GAMEPAD_BUTTON_SOUTH;
			static constexpr SDL_GamepadButton B = SDL_GAMEPAD_BUTTON_EAST;
			static constexpr SDL_GamepadButton X = SDL_GAMEPAD_BUTTON_WEST;
			static constexpr SDL_GamepadButton Y = SDL_GAMEPAD_BUTTON_NORTH;
			static constexpr SDL_GamepadButton LeftShoulder = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
			static constexpr SDL_GamepadButton RightShoulder = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
			static constexpr SDL_GamepadButton LeftStick = SDL_GAMEPAD_BUTTON_LEFT_STICK;
			static constexpr SDL_GamepadButton RightStick = SDL_GAMEPAD_BUTTON_RIGHT_STICK;
			static constexpr SDL_GamepadButton DPadUp = SDL_GAMEPAD_BUTTON_DPAD_UP;
			static constexpr SDL_GamepadButton DPadDown = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
			static constexpr SDL_GamepadButton DPadLeft = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
			static constexpr SDL_GamepadButton DPadRight = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
			static constexpr SDL_GamepadButton Start = SDL_GAMEPAD_BUTTON_START;
			static constexpr SDL_GamepadButton Back = SDL_GAMEPAD_BUTTON_BACK;

		private:
			GamepadButton() = delete;
		};

		/// [EN] Which analog stick an axis-action reads (see BindStick()/ActionAxis2D()).
		/// [JP] 軸アクションがどちらのアナログスティックを読むか（BindStick()/ActionAxis2D() 参照）。
		enum class StickSide
		{
			Left,
			Right
		};

		/// [EN] One WASD/arrow-key-style directional composite: four digital
		///      keys combined into a Vector2 by ActionAxis2D(). An action can
		///      have more than one of these bound (e.g. both WASD and arrow keys).
		/// [JP] WASD/矢印キー的な方向キー1組: 4つのデジタルキーを ActionAxis2D()
		///      が Vector2 に合成する。1つのアクションに複数組バインドできる
		///      （例: WASD と矢印キーの両方）。
		struct DirectionalKeys
		{
			Key up_ = Key::Up;
			Key down_ = Key::Down;
			Key left_ = Key::Left;
			Key right_ = Key::Right;

			Bool operator==(const DirectionalKeys& other)const
			{
				return up_ == other.up_ && down_ == other.down_ && left_ == other.left_ && right_ == other.right_;
			}
		};

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
		static void Initialize();

		/**
		* [EN]
		* Polls input state for the current frame. Call once per frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在フレームの入力状態をポーリングする。毎フレーム1回呼び出す。
		*/
		static void Update();

		/**
		* [EN]
		* Shuts down the input backend.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 入力バックエンドを終了する。
		*/
		static void Finalize();

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
		* Returns whether the virtual key vkey satisfies mode.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 仮想キー vkey が mode を満たすかを返す。
		*/
		static Bool KeyState(Key key, TriggerMode mode = TriggerMode::NONE);

		/**
		* [EN]
		* Returns whether mouse button button satisfies mode.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マウスボタン button が mode を満たすかを返す。
		*/
		static Bool MouseState(MouseButton button, TriggerMode mode = TriggerMode::NONE);

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
		* Accumulates delta into the current frame's mouse wheel total.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* delta を現在フレームのマウスホイール合計に累積する。
		*/
		static void PushMouseWheel(Float delta);

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
		static void BeginMouseCapture();

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
		* Returns whether gamepad button button satisfies mode.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ゲームパッドボタン button が mode を満たすかを返す。
		*/
		static Bool GamepadState(SDL_GamepadButton button, TriggerMode mode = TriggerMode::NONE);

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
		* Rumbles the gamepad's low/high frequency motors for durationMs milliseconds.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ゲームパッドの低周波/高周波モーターを durationMs ミリ秒間振動させる。
		*/
		static void Rumble(Uint16 lowFrequency, Uint16 highFrequency, Uint32 durationMs);

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
		static Bool ActionState(String action, TriggerMode mode = TriggerMode::NONE);

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

		/// [EN] Readable alias for TriggerMode::NONE, for use at KeyState/MouseState/GamepadState call sites.
		/// [JP] TriggerMode::NONE の読みやすいエイリアス。KeyState/MouseState/GamepadState の呼び出し箇所で使う。
		static constexpr TriggerMode IsPressed = TriggerMode::NONE;

		/// [EN] Readable alias for TriggerMode::RISING_EDGE.
		/// [JP] TriggerMode::RISING_EDGE の読みやすいエイリアス。
		static constexpr TriggerMode OnPressed = TriggerMode::RISING_EDGE;

		/// [EN] Readable alias for TriggerMode::FALLING_EDGE.
		/// [JP] TriggerMode::FALLING_EDGE の読みやすいエイリアス。
		static constexpr TriggerMode OnReleased = TriggerMode::FALLING_EDGE;

	private:
		InputSystem() = delete;
	};
}
