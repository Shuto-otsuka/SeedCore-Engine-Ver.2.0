#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Process-wide NVIDIA Nsight Aftermath integration: collects GPU crash
	* dumps (and, when GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo is
	* enabled, shader debug information) so a GPU hang/TDR leaves a
	* `.nv-gpudmp` file that Nsight Graphics can open to show the exact
	* shader/instruction/call stack that was executing - instead of the
	* manual bisection DRED alone requires. Static, process-wide members
	* (like LogSystem) rather than an instance: Aftermath itself is a single
	* per-process facility (GFSDK_Aftermath_EnableGpuCrashDumps/
	* GFSDK_Aftermath_DX12_Initialize only ever apply to the one device the
	* process creates), so there's nothing to inject per-call-site.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体で共有する NVIDIA Nsight Aftermath 統合。GPU クラッシュ
	* ダンプ(および GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo
	* 有効時はシェーダデバッグ情報)を収集し、GPU ハング/TDR 発生時に
	* Nsight Graphics で開けば実行中だった正確なシェーダ/命令/コールスタックが
	* 分かる `.nv-gpudmp` ファイルを残す — DRED 単体が要求する手動の
	* 総当たりを不要にする。LogSystem と同様、インスタンスではなく
	* プロセス全体の static メンバとして持つ: Aftermath 自体がプロセス単位の
	* 機能そのものであり(GFSDK_Aftermath_EnableGpuCrashDumps/
	* GFSDK_Aftermath_DX12_Initialize はプロセスが作る唯一のデバイスにしか
	* 適用されない)、呼び出し箇所ごとに注入するものが無いため。
	*/
	class SEEDCORE_API AftermathCrashTracker
	{
	public:
		/**
		* [EN]
		* Enables Aftermath GPU crash dump collection. Must be called before
		* any D3D12 device is created - crashes on devices created earlier are
		* invisible to Aftermath.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Aftermath の GPU クラッシュダンプ収集を有効化する。D3D12 デバイスを
		* 作成する前に呼ぶ必要がある — それより前に作成されたデバイスの
		* クラッシュは Aftermath から見えない。
		*/
		static void Enable();

		/**
		* [EN]
		* Configures Aftermath for the given device: always tracks resources
		* (so a page fault's faulting VA can be tied back to a resource); the
		* heavier features (event markers with automatic call stack capture,
		* shader debug info generation, extra shader error reporting) are
		* gated behind DEEP_D3D12_DEBUG_MODE, matching D3D12DebugLayer's GPU-
		* Based Validation toggle - both carry real overhead and are meant as
		* temporary diagnostic aids, not always-on.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定デバイスに対して Aftermath を構成する。リソース追跡は常に有効
		* (ページフォルトの発生アドレスをリソースへ結び付けるため)。より
		* 重い機能(自動コールスタック付きイベントマーカー、シェーダデバッグ
		* 情報生成、追加のシェーダエラー報告)は DEEP_D3D12_DEBUG_MODE の
		* 裏に隠す — D3D12DebugLayer の GPU-Based Validation トグルと同じで、
		* どちらも実コストを伴う一時的な診断用途であり、常時有効にはしない。
		*/
		static void Create(ID3D12Device* device);

		/**
		* [EN]
		* Call once device removal/hang is detected (from DeviceRemovedFail).
		* Queries Aftermath's own device status (a finer-grained reason than
		* GetDeviceRemovedReason - Timeout/PageFault/OutOfMemory/DmaFault/...)
		* and polls GFSDK_Aftermath_GetCrashDumpStatus for a bounded time so
		* the crash dump write (done asynchronously by the NVIDIA driver
		* thread) has a chance to finish before the process moves on. Returns
		* a human-readable summary to fold into DeviceRemovedFail's message
		* box, including the crash dump file path once written.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デバイス削除/ハング検知時(DeviceRemovedFail から)に1度呼ぶ。
		* Aftermath 自身のデバイスステータス(GetDeviceRemovedReason より
		* 細かい理由 - Timeout/PageFault/OutOfMemory/DmaFault/…)を照会し、
		* GFSDK_Aftermath_GetCrashDumpStatus を一定時間ポーリングして、
		* (NVIDIA ドライバスレッドが非同期に書く)クラッシュダンプの書き込みが
		* プロセスが先に進む前に完了する機会を与える。DeviceRemovedFail の
		* メッセージボックスへ組み込める人間可読の要約を返す — 書き込み済みの
		* クラッシュダンプファイルパスを含む。
		*/
		static String Report();

		/**
		* [EN]
		* Disables Aftermath GPU crash dump collection. Call once before
		* process shutdown.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Aftermath の GPU クラッシュダンプ収集を無効化する。プロセス終了前に
		* 1度呼ぶ。
		*/
		static void Disable();

	private:
		/// [EN] Guards lastCrashDumpPath_/shaderDebugInfoCount_ against the
		///      free-threaded Aftermath callbacks.
		/// [JP] free-threaded な Aftermath コールバックから
		///      lastCrashDumpPath_/shaderDebugInfoCount_ を保護する。
		static std::mutex mutex_;

		/// [EN] Path of the most recently written crash dump, or empty if
		///      none has been written this process.
		/// [JP] 直近に書き出したクラッシュダンプのパス。このプロセスで
		///      一度も書いていなければ空。
		static String lastCrashDumpPath_;

		/// [EN] Number of shader debug info blobs written this process -
		///      folded into Report()'s summary.
		/// [JP] このプロセスで書き出したシェーダデバッグ情報の数 -
		///      Report() の要約へ含める。
		static Uint32 shaderDebugInfoCount_;

		/// [EN] Directory every crash dump / shader debug info file is
		///      written into, created on first use.
		/// [JP] 全てのクラッシュダンプ/シェーダデバッグ情報ファイルの
		///      書き出し先ディレクトリ。初回使用時に作成する。
		static const Char* DumpDirectory();

		/// [EN] Callback trampolines matching Aftermath's C function pointer
		///      signatures (GFSDK_Aftermath_EnableGpuCrashDumps requires
		///      these, not member functions).
		/// [JP] Aftermath の C 関数ポインタ型に合わせたコールバック本体
		///      (GFSDK_Aftermath_EnableGpuCrashDumps はメンバ関数ではなく
		///      これらを要求する)。
		static void OnCrashDump(const void* gpuCrashDump, Uint32 gpuCrashDumpSize, void* userData);
		static void OnShaderDebugInfo(const void* shaderDebugInfo, Uint32 shaderDebugInfoSize, void* userData);
		static void OnDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addValue, void* userData);
	};
}
