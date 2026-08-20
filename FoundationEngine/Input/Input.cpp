#include <FoundationEngine/Input/Input.h>
#include <FoundationEngine/Serialization/SerializeFallback.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] On-disk shape of one action's bindings (see Input::LoadBindings/SaveBindings).
		///      Key/SDL_GamepadButton round-trip as Int32 since cereal has no
		///      built-in support for raw enums (same convention as GameConfig's
		///      upscaleMode_/resolution_, see Editor/Editor/Build/Config.cpp).
		/// [JP] 1アクション分のバインド情報のディスク上の形（Input::LoadBindings/
		///      SaveBindings 参照）。Key/SDL_GamepadButton は cereal が生の enum を
		///      直接扱えないため Int32 として往復させる（GameConfig の
		///      upscaleMode_/resolution_ と同じ規約、Editor/Editor/Build/Config.cpp 参照）。
		struct AxisKeysRecord
		{
			Int32 up_ = 0;
			Int32 down_ = 0;
			Int32 left_ = 0;
			Int32 right_ = 0;

			template<class Archive>
			void serialize(Archive& archive)
			{
				archive(
					cereal::make_nvp("up", up_),
					cereal::make_nvp("down", down_),
					cereal::make_nvp("left", left_),
					cereal::make_nvp("right", right_)
				);
			}
		};

		struct ActionBindingRecord
		{
			String action_;
			DynamicArray<Int32> keys_;
			DynamicArray<Int32> gamepadButtons_;
			DynamicArray<AxisKeysRecord> axisKeys_;
			DynamicArray<Int32> sticks_;

			template<class Archive>
			void serialize(Archive& archive)
			{
				archive(
					cereal::make_nvp("action", action_),
					cereal::make_nvp("keys", keys_),
					cereal::make_nvp("gamepadButtons", gamepadButtons_),
					cereal::make_nvp("axisKeys", axisKeys_),
					cereal::make_nvp("sticks", sticks_)
				);
			}
		};
	}

	Bool Input::inputEnabled_ = true;
	Bool Input::currentKeys_[KEY_COUNT] = {};
	Bool Input::previousKeys_[KEY_COUNT] = {};
	Bool Input::currentMouse_[MOUSE_BUTTON_COUNT] = {};
	Bool Input::previousMouse_[MOUSE_BUTTON_COUNT] = {};
	Float Input::mouseX_ = 0.0f;
	Float Input::mouseY_ = 0.0f;
	Float Input::mouseDeltaX_ = 0.0f;
	Float Input::mouseDeltaY_ = 0.0f;
	Float Input::mouseWheelDelta_ = 0.0f;
	Float Input::mouseWheelDeltaFrame_ = 0.0f;
	Bool Input::mouseCaptured_ = false;
	Float Input::mouseCaptureAnchorX_ = 0.0f;
	Float Input::mouseCaptureAnchorY_ = 0.0f;
	Float Input::mouseCaptureReturnX_ = 0.0f;
	Float Input::mouseCaptureReturnY_ = 0.0f;
	Bool Input::currentGamepad_[GAMEPAD_BUTTON_COUNT] = {};
	Bool Input::previousGamepad_[GAMEPAD_BUTTON_COUNT] = {};
	Float Input::axisLX_ = 0.0f;
	Float Input::axisLY_ = 0.0f;
	Float Input::axisRX_ = 0.0f;
	Float Input::axisRY_ = 0.0f;
	SDL_Gamepad* Input::gamepad_ = nullptr;

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
	void Input::Initialize()
	{
		SDL_Init(SDL_INIT_GAMEPAD);
		LoadBindings();
	}

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
	void Input::Update()
	{
		/// [EN] The frame's accumulated wheel delta becomes readable via MouseWheelDelta(); start a fresh accumulator for the next frame.
		/// [JP] このフレームで累積されたホイールデルタを MouseWheelDelta() で読み出せるようにし、次フレーム用の累積器を新しく開始する。
		mouseWheelDeltaFrame_ = mouseWheelDelta_;
		mouseWheelDelta_ = 0.0f;

		/// [EN] Drain SDL's event queue solely to detect gamepad hot-plug; keyboard/mouse/axis state is polled directly below instead of via events.
		/// [JP] SDL のイベントキューを、ゲームパッドのホットプラグ検出のためだけに消化する。キーボード/マウス/軸の状態はイベント経由ではなく、以下で直接ポーリングする。
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_GAMEPAD_ADDED:
				if (!gamepad_)
				{
					gamepad_ = SDL_OpenGamepad(event.gdevice.which);
				}
				break;
			case SDL_EVENT_GAMEPAD_REMOVED:
				if (gamepad_)
				{
					SDL_CloseGamepad(gamepad_);
					gamepad_ = nullptr;
				}
				break;
			}
		}

		/// [EN] Snapshot the full Win32 virtual-key table once per frame; fall back to all-zero (nothing pressed) if the query itself fails.
		/// [JP] Win32 の仮想キーテーブル全体を、フレームごとに1回スナップショットする。クエリ自体が失敗した場合は全ゼロ（何も押されていない）にフォールバックする。
		BYTE keyboardState[256];
		if (!GetKeyboardState(keyboardState))
		{
			memset(keyboardState, 0, sizeof(keyboardState));
		}

		/// [EN] Roll last frame's snapshot into previousKeys_ before overwriting currentKeys_, so KeyState's edge detection has something to compare against.
		/// [JP] currentKeys_ を上書きする前に、前フレームのスナップショットを previousKeys_ へ繰り越す。これにより KeyState のエッジ検出が比較対象を持てるようにする。
		for (Int index = 0; index < KEY_COUNT; ++index)
		{
			previousKeys_[index] = currentKeys_[index];
			currentKeys_[index] = (keyboardState[index] & 0x80) != 0;
		}

		/// [EN] Compute this frame's mouse delta from the raw screen-space cursor movement before mouseX_/mouseY_ get overwritten.
		/// [JP] mouseX_/mouseY_ が上書きされる前に、生のスクリーン空間でのカーソル移動から、このフレームのマウスデルタを計算する。
		POINT point;
		GetCursorPos(&point);
		Float newX = static_cast<Float>(point.x);
		Float newY = static_cast<Float>(point.y);
		mouseDeltaX_ = newX - mouseX_;
		mouseDeltaY_ = newY - mouseY_;
		mouseX_ = newX;
		mouseY_ = newY;

		/// [EN] While captured (see BeginMouseCapture), re-anchor the cursor
		///      every frame so it never reaches a monitor edge - GetCursorPos()
		///      clamps there, which would otherwise silently zero out further
		///      delta. This is pure mechanism; deciding when to capture is the
		///      caller's job (e.g. the Editor viewport panel).
		/// [JP] キャプチャ中(BeginMouseCapture 参照)は、毎フレームカーソルを
		///      再アンカーしてモニタ端に到達しないようにする。モニタ端では
		///      GetCursorPos() がクランプされ、それ以上のデルタが暗黙に 0 に
		///      なってしまうため。これは純粋な機構であり、いつキャプチャするか
		///      （例: エディタビューポートパネル）は呼び出し側の判断。
		if (mouseCaptured_)
		{
			SetCursorPos(static_cast<Int>(mouseCaptureAnchorX_), static_cast<Int>(mouseCaptureAnchorY_));
			mouseX_ = mouseCaptureAnchorX_;
			mouseY_ = mouseCaptureAnchorY_;
		}

		/// [EN] Mouse buttons are also read from the same Win32 virtual-key table fetched above, rather than a separate API call.
		/// [JP] マウスボタンも、別途 API 呼び出しをするのではなく、上で取得した同じ Win32 仮想キーテーブルから読み取る。
		static constexpr Int mouseVKeys[MOUSE_BUTTON_COUNT] =
		{
			VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2
		};
		for (Int index = 0; index < MOUSE_BUTTON_COUNT; ++index)
		{
			previousMouse_[index] = currentMouse_[index];
			currentMouse_[index] = (keyboardState[mouseVKeys[index]] & 0x80) != 0;
		}

		/// [EN] Gamepad polling only runs while a gamepad is actually connected (gamepad_ is set by the hot-plug handling above).
		/// [JP] ゲームパッドのポーリングは、実際にゲームパッドが接続されている間のみ実行される（gamepad_ は上のホットプラグ処理によって設定される）。
		if (gamepad_)
		{
			for (Int index = 0; index < GAMEPAD_BUTTON_COUNT; ++index)
			{
				previousGamepad_[index] = currentGamepad_[index];
				currentGamepad_[index] = SDL_GetGamepadButton(gamepad_, static_cast<SDL_GamepadButton>(index));
			}
			axisLX_ = NormalizeAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTX));
			axisLY_ = NormalizeAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTY));
			axisRX_ = NormalizeAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTX));
			axisRY_ = NormalizeAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTY));
		}
	}

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
	void Input::Finalize()
	{
		if (gamepad_)
		{
			SDL_CloseGamepad(gamepad_);
			gamepad_ = nullptr;
		}
		SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
	}

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
	void Input::SetInputEnabled(Bool enabled)
	{
		inputEnabled_ = enabled;
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
	Bool Input::IsInputEnabled()
	{
		return inputEnabled_;
	}

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
	Bool Input::KeyState(Key key, TriggerMode mode)
	{
		if (!inputEnabled_)
		{
			return false;
		}

		Int vkey = static_cast<Int>(key);
		if (vkey < 0 || vkey >= KEY_COUNT)
		{
			return false;
		}

		switch (mode)
		{
		case TriggerMode::RISING_EDGE:  
			return !previousKeys_[vkey] && currentKeys_[vkey];
		case TriggerMode::FALLING_EDGE: 
			return previousKeys_[vkey] && !currentKeys_[vkey];
		case TriggerMode::NONE:       
			return currentKeys_[vkey];
		}
		return false;
	}

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
	Bool Input::MouseState(MouseButton button, TriggerMode mode)
	{
		if (!inputEnabled_)
		{
			return false;
		}

		Int index = static_cast<Int>(button);
		if (index < 0 || index >= MOUSE_BUTTON_COUNT)
		{
			return false;
		}

		switch (mode)
		{
		case TriggerMode::RISING_EDGE:
			return !previousMouse_[index] && currentMouse_[index];
		case TriggerMode::FALLING_EDGE:
			return previousMouse_[index] && !currentMouse_[index];
		case TriggerMode::NONE:
			return currentMouse_[index];
		}
		return false;
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
	Float Input::MouseX()
	{
		return mouseX_;
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
	Float Input::MouseY()
	{
		return mouseY_;
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
	Float Input::MouseDeltaX()
	{
		if (!inputEnabled_)
		{
			return 0.0f;
		}

		return mouseDeltaX_;
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
	Float Input::MouseDeltaY()
	{
		if (!inputEnabled_)
		{
			return 0.0f;
		}

		return mouseDeltaY_;
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
	Float Input::MouseWheelDelta()
	{
		if (!inputEnabled_)
		{
			return 0.0f;
		}

		return mouseWheelDeltaFrame_;
	}

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
	void Input::PushMouseWheel(Float delta)
	{
		mouseWheelDelta_ += delta;
	}

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
	void Input::BeginMouseCapture()
	{
		if (mouseCaptured_)
		{
			return;
		}

		mouseCaptured_ = true;

		/// [EN] Remember the cursor's current position both as the anchor Update() re-snaps to every frame and as the position EndMouseCapture() restores later.
		/// [JP] 現在のカーソル位置を、Update() が毎フレーム再スナップするアンカーとして、また後で EndMouseCapture() が復元する位置として記憶する。
		POINT origin;
		GetCursorPos(&origin);
		mouseCaptureReturnX_ = static_cast<Float>(origin.x);
		mouseCaptureReturnY_ = static_cast<Float>(origin.y);
		mouseCaptureAnchorX_ = mouseCaptureReturnX_;
		mouseCaptureAnchorY_ = mouseCaptureReturnY_;

		ShowCursor(FALSE);
	}

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
	void Input::EndMouseCapture()
	{
		if (!mouseCaptured_)
		{
			return;
		}

		mouseCaptured_ = false;

		ShowCursor(TRUE);
		SetCursorPos(static_cast<Int>(mouseCaptureReturnX_), static_cast<Int>(mouseCaptureReturnY_));
		mouseX_ = mouseCaptureReturnX_;
		mouseY_ = mouseCaptureReturnY_;
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
	Bool Input::IsMouseCaptured()
	{
		return mouseCaptured_;
	}

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
	Bool Input::GamepadState(SDL_GamepadButton button, TriggerMode mode)
	{
		Int index = static_cast<Int>(button);
		if (index < 0 || index >= GAMEPAD_BUTTON_COUNT)
		{
			return false;
		}

		switch (mode)
		{
		case TriggerMode::RISING_EDGE:  
			return !previousGamepad_[index] && currentGamepad_[index];
		case TriggerMode::FALLING_EDGE:
			return previousGamepad_[index] && !currentGamepad_[index];
		case TriggerMode::NONE:        
			return currentGamepad_[index];
		}
		return false;
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
	Float Input::GamepadAxisLX()
	{
		return axisLX_;
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
	Float Input::GamepadAxisLY()
	{
		return axisLY_;
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
	Float Input::GamepadAxisRX()
	{
		return axisRX_;
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
	Float Input::GamepadAxisRY()
	{
		return axisRY_;
	}

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
	void Input::Rumble(Uint16 lowFrequency, Uint16 highFrequency, Uint32 durationMs)
	{
		if (gamepad_)
		{
			SDL_RumbleGamepad(gamepad_, lowFrequency, highFrequency, durationMs);
		}
	}

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
	void Input::RumbleTriggers(Uint16 left, Uint16 right, Uint32 durationMs)
	{
		if (gamepad_)
		{
			SDL_RumbleGamepadTriggers(gamepad_, left, right, durationMs);
		}
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
	Bool Input::ActionState(String action, TriggerMode mode)
	{
		auto iterator = actionBindings_.find(action);
		if (iterator == actionBindings_.end())
		{
			return false;
		}

		for (Key key : iterator->second.keys_)
		{
			if (KeyState(key, mode))
			{
				return true;
			}
		}

		for (SDL_GamepadButton button : iterator->second.gamepadButtons_)
		{
			if (GamepadState(button, mode))
			{
				return true;
			}
		}

		return false;
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
	void Input::RegisterAction(String action)
	{
		GetOrCreateActionBinding(action);
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
	void Input::BindKey(String action, Key key)
	{
		ActionBinding& binding = GetOrCreateActionBinding(action);
		if (std::find(binding.keys_.begin(), binding.keys_.end(), key) == binding.keys_.end())
		{
			binding.keys_.push_back(key);
		}
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
	void Input::BindGamepadButton(String action, SDL_GamepadButton button)
	{
		ActionBinding& binding = GetOrCreateActionBinding(action);
		if (std::find(binding.gamepadButtons_.begin(), binding.gamepadButtons_.end(), button) == binding.gamepadButtons_.end())
		{
			binding.gamepadButtons_.push_back(button);
		}
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
	void Input::UnbindKey(String action, Key key)
	{
		auto iterator = actionBindings_.find(action);
		if (iterator == actionBindings_.end())
		{
			return;
		}

		DynamicArray<Key>& keys = iterator->second.keys_;
		keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
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
	void Input::UnbindGamepadButton(String action, SDL_GamepadButton button)
	{
		auto iterator = actionBindings_.find(action);
		if (iterator == actionBindings_.end())
		{
			return;
		}

		DynamicArray<SDL_GamepadButton>& gamepadButtons = iterator->second.gamepadButtons_;
		gamepadButtons.erase(std::remove(gamepadButtons.begin(), gamepadButtons.end(), button), gamepadButtons.end());
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
	void Input::RemoveAction(String action)
	{
		if (actionBindings_.erase(action))
		{
			actionOrder_.erase(std::remove(actionOrder_.begin(), actionOrder_.end(), action), actionOrder_.end());
		}
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
	const DynamicArray<Input::Key>& Input::GetBoundKeys(String action)
	{
		static const DynamicArray<Key> empty;
		auto iterator = actionBindings_.find(action);
		return iterator != actionBindings_.end() ? iterator->second.keys_ : empty;
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
	const DynamicArray<SDL_GamepadButton>& Input::GetBoundGamepadButtons(String action)
	{
		static const DynamicArray<SDL_GamepadButton> empty;
		auto iterator = actionBindings_.find(action);
		return iterator != actionBindings_.end() ? iterator->second.gamepadButtons_ : empty;
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
	void Input::BindAxisKeys(String action, DirectionalKeys keys)
	{
		ActionBinding& binding = GetOrCreateActionBinding(action);
		if (std::find(binding.axisKeys_.begin(), binding.axisKeys_.end(), keys) == binding.axisKeys_.end())
		{
			binding.axisKeys_.push_back(keys);
		}
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
	void Input::UnbindAxisKeys(String action, DirectionalKeys keys)
	{
		auto iterator = actionBindings_.find(action);
		if (iterator == actionBindings_.end())
		{
			return;
		}

		DynamicArray<DirectionalKeys>& axisKeys = iterator->second.axisKeys_;
		axisKeys.erase(std::remove(axisKeys.begin(), axisKeys.end(), keys), axisKeys.end());
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
	void Input::BindStick(String action, StickSide side)
	{
		ActionBinding& binding = GetOrCreateActionBinding(action);
		if (std::find(binding.sticks_.begin(), binding.sticks_.end(), side) == binding.sticks_.end())
		{
			binding.sticks_.push_back(side);
		}
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
	void Input::UnbindStick(String action, StickSide side)
	{
		auto iterator = actionBindings_.find(action);
		if (iterator == actionBindings_.end())
		{
			return;
		}

		DynamicArray<StickSide>& sticks = iterator->second.sticks_;
		sticks.erase(std::remove(sticks.begin(), sticks.end(), side), sticks.end());
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
	Vector2 Input::ActionAxis2D(String action)
	{
		auto iterator = actionBindings_.find(action);
		if (iterator == actionBindings_.end())
		{
			return Vector2(0.0f, 0.0f);
		}

		Vector2 keyboardVector(0.0f, 0.0f);
		for (const DirectionalKeys& axisKeys : iterator->second.axisKeys_)
		{
			if (KeyState(axisKeys.up_, TriggerMode::NONE))
			{
				keyboardVector.y += 1.0f;
			}
			if (KeyState(axisKeys.down_, TriggerMode::NONE))
			{
				keyboardVector.y -= 1.0f;
			}
			if (KeyState(axisKeys.left_, TriggerMode::NONE))
			{
				keyboardVector.x -= 1.0f;
			}
			if (KeyState(axisKeys.right_, TriggerMode::NONE))
			{
				keyboardVector.x += 1.0f;
			}
		}

		if (keyboardVector.x != 0.0f || keyboardVector.y != 0.0f)
		{
			keyboardVector.Normalize();
			return keyboardVector;
		}

		for (StickSide side : iterator->second.sticks_)
		{
			/// [EN] SDL reports stick-down as positive Y; flip so "up" on the
			///      stick matches +Y here, same as the keyboard composite above.
			/// [JP] SDL はスティック下方向を正のYとして報告するため、符号を
			///      反転し、上記のキーボード合成と同じく「上」が +Y になる
			///      ようにする。
			Float x = (side == StickSide::Left) ? GamepadAxisLX() : GamepadAxisRX();
			Float y = (side == StickSide::Left) ? -GamepadAxisLY() : -GamepadAxisRY();
			if (x != 0.0f || y != 0.0f)
			{
				return Vector2(x, y);
			}
		}

		return Vector2(0.0f, 0.0f);
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
	const DynamicArray<Input::DirectionalKeys>& Input::GetBoundAxisKeys(String action)
	{
		static const DynamicArray<DirectionalKeys> empty;
		auto iterator = actionBindings_.find(action);
		return iterator != actionBindings_.end() ? iterator->second.axisKeys_ : empty;
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
	const DynamicArray<Input::StickSide>& Input::GetBoundSticks(String action)
	{
		static const DynamicArray<StickSide> empty;
		auto iterator = actionBindings_.find(action);
		return iterator != actionBindings_.end() ? iterator->second.sticks_ : empty;
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
	const DynamicArray<String>& Input::GetActionNames()
	{
		return actionOrder_;
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
	void Input::LoadBindings(const std::filesystem::path& path)
	{
		std::ifstream ifs(path);
		if (!ifs)
		{
			return;
		}

		DynamicArray<ActionBindingRecord> records;

		try
		{
			cereal::JSONInputArchive archive(ifs);
			TryLoadField(archive, "bindings", records);
		}
		catch (...)
		{
			return;
		}

		actionBindings_.clear();
		actionOrder_.clear();

		for (const ActionBindingRecord& record : records)
		{
			ActionBinding& binding = GetOrCreateActionBinding(record.action_);

			for (Int32 key : record.keys_)
			{
				binding.keys_.push_back(static_cast<Key>(key));
			}

			for (Int32 button : record.gamepadButtons_)
			{
				binding.gamepadButtons_.push_back(static_cast<SDL_GamepadButton>(button));
			}

			for (const AxisKeysRecord& axisKeysRecord : record.axisKeys_)
			{
				DirectionalKeys axisKeys;
				axisKeys.up_ = static_cast<Key>(axisKeysRecord.up_);
				axisKeys.down_ = static_cast<Key>(axisKeysRecord.down_);
				axisKeys.left_ = static_cast<Key>(axisKeysRecord.left_);
				axisKeys.right_ = static_cast<Key>(axisKeysRecord.right_);
				binding.axisKeys_.push_back(axisKeys);
			}

			for (Int32 side : record.sticks_)
			{
				binding.sticks_.push_back(static_cast<StickSide>(side));
			}
		}
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
	void Input::SaveBindings(const std::filesystem::path& path)
	{
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		std::ofstream ofs(path);
		if (!ofs)
		{
			return;
		}

		DynamicArray<ActionBindingRecord> records;
		records.reserve(actionOrder_.size());

		for (const String& action : actionOrder_)
		{
			const ActionBinding& binding = actionBindings_.at(action);

			ActionBindingRecord record;
			record.action_ = action;

			for (Key key : binding.keys_)
			{
				record.keys_.push_back(static_cast<Int32>(key));
			}

			for (SDL_GamepadButton button : binding.gamepadButtons_)
			{
				record.gamepadButtons_.push_back(static_cast<Int32>(button));
			}

			for (const DirectionalKeys& axisKeys : binding.axisKeys_)
			{
				AxisKeysRecord axisKeysRecord;
				axisKeysRecord.up_ = static_cast<Int32>(axisKeys.up_);
				axisKeysRecord.down_ = static_cast<Int32>(axisKeys.down_);
				axisKeysRecord.left_ = static_cast<Int32>(axisKeys.left_);
				axisKeysRecord.right_ = static_cast<Int32>(axisKeys.right_);
				record.axisKeys_.push_back(axisKeysRecord);
			}

			for (StickSide side : binding.sticks_)
			{
				record.sticks_.push_back(static_cast<Int32>(side));
			}

			records.push_back(std::move(record));
		}

		cereal::JSONOutputArchive archive(ofs);
		archive(cereal::make_nvp("bindings", records));
	}

	/**
	* [EN]
	* Returns actionBindings_'s entry for action, creating an empty one
	* (and appending to actionOrder_) if action is new.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* action に対応する actionBindings_ のエントリを返す。action が未知
	* なら空のエントリを作成し（actionOrder_ にも追加し）返す。
	*/
	Input::ActionBinding& Input::GetOrCreateActionBinding(const String& action)
	{
		auto iterator = actionBindings_.find(action);
		if (iterator != actionBindings_.end())
		{
			return iterator->second;
		}

		actionOrder_.push_back(action);
		return actionBindings_[action];
	}

	/**
	* [EN]
	* Normalizes a raw SDL stick axis value (Sint16 range) to [-1, 1].
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 生のSDLスティック軸の値（Sint16 の範囲）を [-1, 1] に正規化する。
	*/
	Float Input::NormalizeAxis(Sint16 value)
	{
		return Clamp(static_cast<Float>(value) / 32768.0f, -1.0f, 1.0f);
	}
}
