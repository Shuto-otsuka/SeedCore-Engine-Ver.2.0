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

		void Draw(ID3D12GraphicsCommandList6* cmdList, ID3D12Resource* backBuffer, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, Float screenWidth, Float screenHeight, Bool loadComplete, Float progress, Bool showWarning, Bool showFiction);

		[[nodiscard]] Bool IsFinished()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> dayResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> nightResource_;

		Uint dayTextureIndex_ = 0;
		Uint nightTextureIndex_ = 0;

		/// [EN] The warning/fiction disclaimer screens, shown (if showWarning_/
		///      showFiction_) before the logo, in that order - each a single
		///      centered/letterboxed image drawn through the same t0 slot and
		///      texture_aspect_/show_logo_ path as the day/night logo (see
		///      Draw()'s phase selection). Runtime/Logo/seedcore_warning_notice.dds
		///      and seedcore_fiction_notice.dds.
		/// [JP] 警告/フィクション免責画面。showWarning_/showFiction_が立っていれば
		///      ロゴの前に、その順で表示される - どちらも day/night ロゴと同じ
		///      t0スロット・texture_aspect_/show_logo_の経路で描画される単一の
		///      中央寄せ/レターボックス画像（Draw()のフェーズ選択参照）。
		///      Runtime/Logo/seedcore_warning_notice.dds と seedcore_fiction_notice.dds。
		Microsoft::WRL::ComPtr<ID3D12Resource> warningResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> fictionResource_;

		Uint warningTextureIndex_ = 0;
		Uint fictionTextureIndex_ = 0;

		/// [EN] The CRI ADX2 middleware attribution logo, shown after Fiction and
		///      before the engine's own day/night logo (always on, unlike
		///      warning/fiction - middleware attribution isn't optional). Drawn
		///      through the same t0 slot/path as the others. Runtime/Logo/criware_logo.dds.
		/// [JP] CRI ADX2 ミドルウェアのクレジットロゴ。Fiction の後、エンジン
		///      自体の昼夜ロゴの前に表示される（warning/fictionと違い常時表示 -
		///      ミドルウェアのクレジット表記は任意ではないため）。他と同じ
		///      t0スロット/経路で描画される。Runtime/Logo/criware_logo.dds。
		Microsoft::WRL::ComPtr<ID3D12Resource> criLogoResource_;

		Uint criLogoTextureIndex_ = 0;

		/// [EN] The loading-phase background - drawn behind the progress bar,
		///      covering the full screen (crop-to-fill, not letterboxed like the
		///      logo/bar - see SplashScreenPS.hlsl). Runtime/Logo/seedcore_progress_background.dds.
		/// [JP] ロード演出フェーズの背景 - 進捗バーの後ろに、画面全体を覆うように
		///      描画される（ロゴ/バーと違いレターボックスせず、はみ出す分は
		///      クロップする - SplashScreenPS.hlsl参照）。
		///      Runtime/Logo/seedcore_progress_background.dds。
		Microsoft::WRL::ComPtr<ID3D12Resource> progressBackgroundResource_;

		Uint progressBackgroundTextureIndex_ = 0;

		/// [EN] The loading-phase progress bar's fill sprite and its border/frame
		///      overlay - drawn after the logo fades out, masked by the load
		///      progress ratio (see SplashScreenPS.hlsl). Runtime/Logo/seedcore_progress_bar.dds and seedcore_progress_frame.dds.
		/// [JP] ロード演出フェーズの進捗バーの塗りつぶしスプライトと、その縁取り/
		///      フレームのオーバーレイ - ロゴがフェードアウトした後に描画され、
		///      ロード進捗率でマスクされる（SplashScreenPS.hlsl参照）。
		///      Runtime/Logo/seedcore_progress_bar.dds と seedcore_progress_frame.dds。
		Microsoft::WRL::ComPtr<ID3D12Resource> progressBarResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> progressFrameResource_;

		Uint progressBarTextureIndex_ = 0;
		Uint progressFrameTextureIndex_ = 0;

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

		/// [EN] Whether the warning/fiction disclaimer screens should play as
		///      part of this sequence (Warning -> Fiction -> CRI logo -> Engine
		///      logo -> Progress, each of the first two phases skipped entirely
		///      if its flag is false) - set from Draw()'s showWarning/showFiction
		///      parameters on the first call, mirroring started_. Runtime export
		///      will eventually drive these from the export dialog's two
		///      checkboxes (Editor::MenuBarPanel).
		/// [JP] 警告/フィクション免責画面をこのシーケンスの一部として再生するか
		///      どうか（Warning -> Fiction -> CRIロゴ -> エンジンロゴ -> Progress
		///      の順で、最初の2フェーズはフラグがfalseならそのフェーズ自体を
		///      丸ごとスキップする） - started_ と同様、初回の Draw() 呼び出し時に
		///      showWarning/showFiction 引数から設定する。将来的には Runtime
		///      書き出し時のエクスポートダイアログの2つのチェックボックス
		///      （Editor::MenuBarPanel）から駆動する想定。
		Bool showWarning_ = false;
		Bool showFiction_ = false;

		BindlessHeap* bindlessHeap_ = nullptr;
		D3D12CommandQueue* cmdQueue_ = nullptr;

		std::chrono::steady_clock::time_point startTime_;
	};
}
