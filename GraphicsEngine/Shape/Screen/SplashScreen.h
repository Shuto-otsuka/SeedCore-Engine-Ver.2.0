#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class TextureLoader;
	class BindlessHeap;
	class D3D12CommandQueue;

	class SplashScreen
	{
	public:
		SplashScreen() = default;
		~SplashScreen() = default;

		void Initialize(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap);

		void Draw(ID3D12GraphicsCommandList6* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, Float screenWidth, Float screenHeight, Bool showWarning, Bool showFiction);

		[[nodiscard]] Bool Finished()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> dayResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> nightResource_;

		Uint dayTextureIndex_ = 0;
		Uint nightTextureIndex_ = 0;

		/// [EN] The warning/fiction disclaimer screens, shown (if showWarning_/
		///      showFiction_) before the logo, in that order - each a single
		///      centered/letterboxed image drawn through the same t0 slot and
		///      texture_aspect_ path as the day/night logo (see Draw()'s phase
		///      selection). Runtime/Logo/Warning.sub.logo and Fiction.sub.logo.
		/// [JP] 警告/フィクション免責画面。showWarning_/showFiction_が立っていれば
		///      ロゴの前に、その順で表示される - どちらも day/night ロゴと同じ
		///      t0スロット・texture_aspect_の経路で描画される単一の
		///      中央寄せ/レターボックス画像（Draw()のフェーズ選択参照）。
		///      Runtime/Logo/Warning.sub.logo と Fiction.sub.logo。
		Microsoft::WRL::ComPtr<ID3D12Resource> warningResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> fictionResource_;

		Uint warningTextureIndex_ = 0;
		Uint fictionTextureIndex_ = 0;

		/// [EN] The CRI ADX2 middleware attribution logo, shown after Fiction and
		///      before the engine's own day/night logo (always on, unlike
		///      warning/fiction - middleware attribution isn't optional). Drawn
		///      through the same t0 slot/path as the others. Runtime/Logo/CriWare.logo.
		/// [JP] CRI ADX2 ミドルウェアのクレジットロゴ。Fiction の後、エンジン
		///      自体の昼夜ロゴの前に表示される（warning/fictionと違い常時表示 -
		///      ミドルウェアのクレジット表記は任意ではないため）。他と同じ
		///      t0スロット/経路で描画される。Runtime/Logo/CriWare.logo。
		Microsoft::WRL::ComPtr<ID3D12Resource> criLogoResource_;

		Uint criLogoTextureIndex_ = 0;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

		Float minDuration_ = 3.0f;
		Float warningDuration_ = 2.5f;
		Float fictionDuration_ = 2.5f;
		Float criLogoDuration_ = 2.5f;
		Float fadeInTime_ = 0.5f;
		Float fadeOutTime_ = 0.5f;

		Bool finished_ = false;
		Bool initialized_ = false;
		Bool started_ = false;

		/// [EN] Whether the warning/fiction disclaimer screens play as part of
		///      this sequence (Warning -> Fiction -> CRI logo -> Engine logo,
		///      each of the first two phases skipped entirely if its flag is
		///      false) - set from Draw()'s showWarning/showFiction parameters on
		///      the first call, mirroring started_. The Runtime passes
		///      GameConfig's showSplashWarning_/showSplashFiction_; the Editor
		///      passes false for both.
		/// [JP] 警告/フィクション免責画面をこのシーケンスの一部として再生するか
		///      どうか（Warning -> Fiction -> CRIロゴ -> エンジンロゴ の順で、
		///      最初の2フェーズはフラグがfalseならそのフェーズ自体を丸ごと
		///      スキップする） - started_ と同様、初回の Draw() 呼び出し時に
		///      showWarning/showFiction 引数から設定する。Runtime は GameConfig の
		///      showSplashWarning_/showSplashFiction_ を、Editor は両方 false を渡す。
		Bool showWarning_ = false;
		Bool showFiction_ = false;

		BindlessHeap* bindlessHeap_ = nullptr;
		D3D12CommandQueue* cmdQueue_ = nullptr;

		std::chrono::steady_clock::time_point startTime_;
	};
}
