#include <FoundationEngine/Log/AftermathCrashTracker.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Notice.h>

namespace SeedCore
{
	std::mutex AftermathCrashTracker::mutex_;
	String AftermathCrashTracker::lastCrashDumpPath_;
	Uint32 AftermathCrashTracker::shaderDebugInfoCount_ = 0;

	const Char* AftermathCrashTracker::DumpDirectory()
	{
		static const Char* directory = "../Logs/GpuCrashDumps";
		std::filesystem::create_directories(directory);
		return directory;
	}

	void AftermathCrashTracker::Enable()
	{
		GFSDK_Aftermath_Result result = GFSDK_Aftermath_EnableGpuCrashDumps(
			GFSDK_Aftermath_Version_API,
			GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX,
			GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
			OnCrashDump,
			OnShaderDebugInfo,
			OnDescription,
			nullptr,
			nullptr);

		if (!GFSDK_Aftermath_SUCCEED(result))
		{
			SC_LOG_WARNING("Nsight Aftermath の有効化に失敗しました(結果コード: {:#010x})。対応する NVIDIA GPU/ドライバでない可能性があります。GPU クラッシュダンプは収集されません。", static_cast<Uint32>(result));
		}
	}

	void AftermathCrashTracker::Create(ID3D12Device* device)
	{
		/// [EN] GenerateShaderDebugInfo is always on, unlike the flags below: the debug info DXC embeds (-Qembed_debug)
		///      only reaches a crash dump's shader location when Aftermath is told to capture it as well.
		/// [JP] GenerateShaderDebugInfo は下のフラグと違って常に有効にする。DXC が埋め込むデバッグ情報(-Qembed_debug)は、
		///      Aftermath にも収集させないとクラッシュダンプのシェーダ位置に出ない。
		Uint32 flags = GFSDK_Aftermath_FeatureFlags_EnableResourceTracking | GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo;

#if DEEP_D3D12_DEBUG_MODE
		flags |= GFSDK_Aftermath_FeatureFlags_EnableMarkers;
		flags |= GFSDK_Aftermath_FeatureFlags_CallStackCapturing;
		flags |= GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting;
#endif

		GFSDK_Aftermath_Result result = GFSDK_Aftermath_DX12_Initialize(GFSDK_Aftermath_Version_API, flags, device);
		if (!GFSDK_Aftermath_SUCCEED(result))
		{
			SC_LOG_WARNING("Nsight Aftermath のデバイス初期化に失敗しました(結果コード: {:#010x})。GPU クラッシュダンプは収集されません。", static_cast<Uint32>(result));
			return;
		}

		SC_LOG_NOTICE("Nsight Aftermath を有効化しました。GPU デバイス削除時は '{}' にクラッシュダンプを書き出します。", DumpDirectory());
	}

	String AftermathCrashTracker::Report()
	{
		std::string output;

		GFSDK_Aftermath_Device_Status deviceStatus = GFSDK_Aftermath_Device_Status_Unknown;
		GFSDK_Aftermath_Result deviceStatusResult = GFSDK_Aftermath_GetDeviceStatus(&deviceStatus);
		if (GFSDK_Aftermath_SUCCEED(deviceStatusResult))
		{
			/// [EN] Finer than DXGI's GetDeviceRemovedReason: tells a shader/traversal timeout apart from a page fault, OOM, or a removal with no GPU fault (driver crash, external reset).
			/// [JP] DXGI の GetDeviceRemovedReason より細かく、シェーダ/走査のタイムアウトを、ページフォルト・メモリ不足・GPU 障害の無いデバイス削除(ドライバクラッシュ、外部リセット)と区別する。
			const Char* statusText = "Unknown";
			switch (deviceStatus)
			{
			case GFSDK_Aftermath_Device_Status_Active:                  statusText = "Active(正常)"; break;
			case GFSDK_Aftermath_Device_Status_Timeout:                 statusText = "Timeout(長時間実行しているシェーダ/処理によるタイムアウト)"; break;
			case GFSDK_Aftermath_Device_Status_OutOfMemory:             statusText = "OutOfMemory"; break;
			case GFSDK_Aftermath_Device_Status_PageFault:               statusText = "PageFault(不正な GPU 仮想アドレスアクセス)"; break;
			case GFSDK_Aftermath_Device_Status_Stopped:                 statusText = "Stopped"; break;
			case GFSDK_Aftermath_Device_Status_Reset:                   statusText = "Reset"; break;
			case GFSDK_Aftermath_Device_Status_DmaFault:                statusText = "DmaFault(不正なレンダリング呼び出し)"; break;
			case GFSDK_Aftermath_Device_Status_DeviceRemovedNoGpuFault:  statusText = "DeviceRemovedNoGpuFault(GPU 側の障害を伴わないデバイス削除 - ドライバクラッシュや外部リセットの可能性)"; break;
			default:                                                     statusText = "Unknown(未対応ドライバの可能性)"; break;
			}
			output += std::format("\n\nNsight Aftermath デバイスステータス: {}", statusText);
		}
		else
		{
			output += std::format("\n\nNsight Aftermath デバイスステータス: 取得できません({:#010x})。Aftermath が有効化されていない、または未対応の GPU/ドライバの可能性があります。", static_cast<Uint32>(deviceStatusResult));
		}

		/// [EN] The dump is written asynchronously on an NVIDIA driver thread; poll with a time limit so a driver that never finishes cannot hang this message box.
		/// [JP] ダンプは NVIDIA ドライバのスレッドで非同期に書かれる。終わらないドライバでこのメッセージボックスが止まらないよう、上限付きでポーリングする。
		GFSDK_Aftermath_CrashDump_Status crashDumpStatus = GFSDK_Aftermath_CrashDump_Status_Unknown;
		GFSDK_Aftermath_GetCrashDumpStatus(&crashDumpStatus);

		auto waitStart = std::chrono::steady_clock::now();
		constexpr auto crashDumpTimeout = std::chrono::milliseconds(5000);

		while (crashDumpStatus != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
			crashDumpStatus != GFSDK_Aftermath_CrashDump_Status_Finished &&
			crashDumpStatus != GFSDK_Aftermath_CrashDump_Status_Unknown &&
			std::chrono::steady_clock::now() - waitStart < crashDumpTimeout)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			GFSDK_Aftermath_GetCrashDumpStatus(&crashDumpStatus);
		}

		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!lastCrashDumpPath_.str().empty())
			{
				output += std::format("\n\nNsight Aftermath クラッシュダンプ: {}", lastCrashDumpPath_.str());
				if (shaderDebugInfoCount_ > 0)
				{
					output += std::format("\n(シェーダデバッグ情報 {} 個を同ディレクトリへ書き出し済み。Nsight Graphics でこのダンプを開く際に指定してください)", shaderDebugInfoCount_);
				}
			}
			else
			{
				output += "\n\nNsight Aftermath クラッシュダンプ: 書き出されませんでした(未対応の GPU/ドライバ、または EnableCrashDumps 未実行の可能性)";
			}
		}

		return String(output);
	}

	void AftermathCrashTracker::Disable()
	{
		GFSDK_Aftermath_DisableGpuCrashDumps();
	}

	void AftermathCrashTracker::OnCrashDump(const void* gpuCrashDump, Uint32 gpuCrashDumpSize, void* userData)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm localNow{};
		localtime_s(&localNow, &now);

		std::ostringstream timestamp;
		timestamp << std::put_time(&localNow, "%Y%m%d_%H%M%S");

		std::string path = std::format("{}/{}.nv-gpudmp", DumpDirectory(), timestamp.str());

		std::ofstream file(path, std::ios::binary);
		if (file)
		{
			file.write(static_cast<const Char*>(gpuCrashDump), gpuCrashDumpSize);
		}

		lastCrashDumpPath_ = String(path);
	}

	void AftermathCrashTracker::OnShaderDebugInfo(const void* shaderDebugInfo, Uint32 shaderDebugInfoSize, void* userData)
	{
		GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier{};
		GFSDK_Aftermath_Result result = GFSDK_Aftermath_GetShaderDebugInfoIdentifier(GFSDK_Aftermath_Version_API, shaderDebugInfo, shaderDebugInfoSize, &identifier);
		if (!GFSDK_Aftermath_SUCCEED(result))
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);

		std::string path = std::format("{}/{:016x}{:016x}.nvdbg", DumpDirectory(), identifier.id[0], identifier.id[1]);

		std::ofstream file(path, std::ios::binary);
		if (file)
		{
			file.write(static_cast<const Char*>(shaderDebugInfo), shaderDebugInfoSize);
			shaderDebugInfoCount_++;
		}
	}

	void AftermathCrashTracker::OnDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addValue, void* userData)
	{
		addValue(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "SeedCore Engine");
	}
}
