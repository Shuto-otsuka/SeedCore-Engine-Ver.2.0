#include <GraphicsEngine/Profiler/GpuProfiler.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	/**
	* [EN]
	* Allocates the timestamp query heap (one frame's worth of slots) and the
	* frame-ringed readback buffer, and caches the queue's timestamp frequency.
	* On any failure the profiler stays unavailable and every entry point turns
	* into a no-op, so callers never need to guard their Begin/End calls.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タイムスタンプクエリヒープ(1フレーム分のスロット)とフレームリングの
	* リードバックバッファを確保し、キューのタイムスタンプ周波数をキャッシュする。
	* 失敗した場合はプロファイラを無効のままにして全エントリポイントを no-op に
	* するので、呼び出し側は Begin/End をガードしなくてよい。
	*/
	void GpuProfiler::Create(ID3D12Device* device, ID3D12CommandQueue* commandQueue, Uint32 frameCount)
	{
		available_ = false;

		if (!device || !commandQueue)
		{
			return;
		}

		/// [JP] 読み出しは frameCount-1 フレーム前の領域を対象にするので、
		///      最低 3 にして2フレーム以上の余裕を作る。
		frameCount_ = frameCount < 3 ? 3 : frameCount;
		if (frameCount_ > maxFrameCount_)
		{
			frameCount_ = maxFrameCount_;
		}

		if (FAILED(commandQueue->GetTimestampFrequency(&timestampFrequency_)) || timestampFrequency_ == 0)
		{
			SC_LOG_WARNING("GPU タイムスタンプ周波数を取得できませんでした。GPU プロファイラを無効化します。");
			return;
		}

		D3D12_QUERY_HEAP_DESC queryHeapDesc{};
		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		queryHeapDesc.Count = slotCount_;

		if (FAILED(device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap_))))
		{
			SC_LOG_WARNING("タイムスタンプクエリヒープの生成に失敗しました。GPU プロファイラを無効化します。");
			return;
		}

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = static_cast<Uint64>(frameCount_) * slotCount_ * sizeof(Uint64);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		/// [JP] READBACK ヒープのバッファは COPY_DEST 固定で生成し、以後遷移
		///      させない(ResolveQueryData の要求状態がそれ)。
		if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackResource_))))
		{
			SC_LOG_WARNING("タイムスタンプ読み戻しバッファの生成に失敗しました。GPU プロファイラを無効化します。");
			queryHeap_.Reset();
			return;
		}
#ifdef _DEBUG
		readbackResource_->SetName(L"GpuProfiler_Readback");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(readbackResource_.Get());
#endif

		writeFrame_ = 0;
		advanceCount_ = 0;
		available_ = true;
	}

	/**
	* [EN]
	* Resolves the frame that just finished, reads back the oldest completed
	* frame, and rotates the ring.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直前に終わったフレームを resolve し、完了済みの最古フレームを読み戻して
	* リングを回す。
	*/
	void GpuProfiler::Advance(D3D12CommandList* cmdList)
	{
		if (!available_ || !cmdList)
		{
			return;
		}

		auto* cmd = cmdList->Get();
		if (!cmd)
		{
			return;
		}

		/// [JP] 初回はまだ記録されたタイムスタンプが無いので resolve しない。
		///      代わりに全スロットを一度書いて初期化しておく。ResolveQueryData は
		///      毎フレーム全スロットを対象にする一方、無効なパスや計測していない
		///      ビュー(Canvas など)のスロットは一度も書かれない。未書き込みの
		///      タイムスタンプを resolve すると内容が未定義になり、デバッグレイヤも
		///      警告を出す。値自体は recorded_ のマスクで捨てているが、未定義領域を
		///      読むこと自体を避けておく。
		if (advanceCount_ == 0)
		{
			for (Uint32 slot = 0; slot < slotCount_; slot++)
			{
				cmd->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot);
			}
		}
		else
		{
			cmd->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, slotCount_, readbackResource_.Get(), static_cast<Uint64>(writeFrame_) * slotCount_ * sizeof(Uint64));
		}

		/// [JP] 次に上書きする領域=最も古い領域。そこは frameCount_-1 フレーム前に
		///      resolve を記録した分なので GPU は確実に書き終えている。
		Uint32 readFrame = (writeFrame_ + 1) % frameCount_;

		if (advanceCount_ >= frameCount_)
		{
			const Uint64 regionBytes = static_cast<Uint64>(slotCount_) * sizeof(Uint64);
			const Uint64 regionBegin = static_cast<Uint64>(readFrame) * regionBytes;

			/// [JP] READBACK は読む範囲を明示して Map/Unmap する(ドライバに
			///      キャッシュ整合の範囲を伝えるため)。毎フレーム1回なので
			///      コストは無視できる。
			D3D12_RANGE readRange{};
			readRange.Begin = static_cast<SIZE_T>(regionBegin);
			readRange.End = static_cast<SIZE_T>(regionBegin + regionBytes);

			void* mapped = nullptr;
			if (SUCCEEDED(readbackResource_->Map(0, &readRange, &mapped)) && mapped)
			{
				const Uint64* timestamps = reinterpret_cast<const Uint64*>(static_cast<const Byte*>(mapped) + regionBegin);

				for (Uint32 viewIndex = 0; viewIndex < viewCount_; viewIndex++)
				{
					Uint32 mask = recorded_[readFrame][viewIndex];

					for (Uint32 scopeIndex = 0; scopeIndex < scopeCount_; scopeIndex++)
					{
						if ((mask & (1u << scopeIndex)) == 0)
						{
							milliseconds_[viewIndex][scopeIndex] = 0.0f;
							continue;
						}

						Uint32 beginSlot = (viewIndex * scopeCount_ + scopeIndex) * 2;
						Uint64 beginTick = timestamps[beginSlot];
						Uint64 endTick = timestamps[beginSlot + 1];

						/// [JP] タイムスタンプが逆順/同値になるケース(計測不能)は
						///      0 として扱う。負値を Uint64 で引くと巨大値になる。
						Uint64 elapsed = endTick > beginTick ? endTick - beginTick : 0;
						milliseconds_[viewIndex][scopeIndex] = static_cast<Float>(static_cast<Double>(elapsed) * 1000.0 / static_cast<Double>(timestampFrequency_));
					}
				}

				D3D12_RANGE writtenRange{};
				readbackResource_->Unmap(0, &writtenRange);
			}
		}

		writeFrame_ = readFrame;
		++advanceCount_;

		for (Uint32 viewIndex = 0; viewIndex < viewCount_; viewIndex++)
		{
			recorded_[writeFrame_][viewIndex] = 0;
		}
	}

	void GpuProfiler::Begin(D3D12CommandList* cmdList, GpuProfileView view, GpuProfileScope scope)
	{
		if (!available_ || !cmdList)
		{
			return;
		}

		auto* cmd = cmdList->Get();
		if (!cmd)
		{
			return;
		}

		/// [JP] タイムスタンプクエリは BeginQuery を取らず EndQuery のみで
		///      「その時点の時刻を書く」という使い方をする(D3D12 の仕様)。
		cmd->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, BeginSlot(view, scope));
	}

	void GpuProfiler::End(D3D12CommandList* cmdList, GpuProfileView view, GpuProfileScope scope)
	{
		if (!available_ || !cmdList)
		{
			return;
		}

		auto* cmd = cmdList->Get();
		if (!cmd)
		{
			return;
		}

		cmd->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, BeginSlot(view, scope) + 1);

		/// [JP] End 側で記録済みビットを立てる。Begin だけ通って End が通らない
		///      経路(途中 return など)を「計測済み」と誤認しないため。
		recorded_[writeFrame_][static_cast<Uint32>(view)] |= 1u << static_cast<Uint32>(scope);
	}

	Float GpuProfiler::GetMilliseconds(GpuProfileView view, GpuProfileScope scope)const
	{
		if (!available_)
		{
			return 0.0f;
		}

		return milliseconds_[static_cast<Uint32>(view)][static_cast<Uint32>(scope)];
	}

	Float GpuProfiler::GetViewMilliseconds(GpuProfileView view)const
	{
		if (!available_)
		{
			return 0.0f;
		}

		Float total = 0.0f;
		Uint32 viewIndex = static_cast<Uint32>(view);

		for (Uint32 scopeIndex = 0; scopeIndex < scopeCount_; scopeIndex++)
		{
			total += milliseconds_[viewIndex][scopeIndex];
		}

		return total;
	}

	Bool GpuProfiler::IsAvailable()const
	{
		return available_;
	}

	Uint32 GpuProfiler::BeginSlot(GpuProfileView view, GpuProfileScope scope)const
	{
		return (static_cast<Uint32>(view) * scopeCount_ + static_cast<Uint32>(scope)) * 2;
	}

	const Char* GpuProfiler::ScopeName(GpuProfileScope scope)
	{
		switch (scope)
		{
		case GpuProfileScope::DepthPrepass:					
			return "深度プリパス";
		case GpuProfileScope::HiZBuild:						
			return "Hi-Z 構築";
		case GpuProfileScope::GeometryBuffer:
			return "G-Buffer";
		case GpuProfileScope::MaterialResolve:
			return "VisBuffer マテリアル解決";
		case GpuProfileScope::LightCluster:
			return "ライトクラスタ";
		case GpuProfileScope::RaytraceShadow:				
			return "RT 影";
		case GpuProfileScope::RaytraceAmbientOcclusion:		
			return "RT AO";
		case GpuProfileScope::RaytraceSubsurfaceScattering:	
			return "RT 表面下散乱";
		case GpuProfileScope::RaytraceReflection:
			return "RT 反射";
		case GpuProfileScope::RaytraceRefraction:
			return "RT 屈折";
		case GpuProfileScope::RaytraceGlobalIllumination:
			return "RT GI";
		case GpuProfileScope::VolumetricCloudScapes:
			return "RT 雲";
		case GpuProfileScope::VolumetricStar:
			return "RT 星";
		case GpuProfileScope::WeatherParticle:
			return "天候パーティクル";
		case GpuProfileScope::VolumetricLight:
			return "RT ボリュメトリックライト";
		case GpuProfileScope::SkyGenerate:
			return "空 IBL 生成";
		case GpuProfileScope::Composite:
			return "ライティング合成";
		case GpuProfileScope::Transparent:
			return "半透明";
		case GpuProfileScope::DlssRayReconstruction:
			return "DLSS-RR";
		case GpuProfileScope::Taau:
			return "TAAU";
		case GpuProfileScope::PostProcess:
			return "ポストプロセス";
		default:											
			return "不明";
		}
	}

	const Char* GpuProfiler::ViewName(GpuProfileView view)
	{
		switch (view)
		{
		case GpuProfileView::Editor:	
			return "エディタ";
		case GpuProfileView::Game:		
			return "ゲーム";
		case GpuProfileView::Canvas:	
			return "キャンバス";
		default:						
			return "不明";
		}
	}

	GpuProfileScopeGuard::GpuProfileScopeGuard(GpuProfiler& profiler, D3D12CommandList* cmdList, GpuProfileView view, GpuProfileScope scope) : profiler_(profiler), cmdList_(cmdList), view_(view), scope_(scope)
	{
		profiler_.Begin(cmdList_, view_, scope_);
	}

	GpuProfileScopeGuard::~GpuProfileScopeGuard()
	{
		profiler_.End(cmdList_, view_, scope_);
	}
}
