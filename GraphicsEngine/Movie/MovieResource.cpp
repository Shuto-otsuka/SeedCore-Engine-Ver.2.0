#include <GraphicsEngine/Movie/MovieResource.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	Bool MovieResource::Entry::Initialize(const std::string& filePath)
	{
		Finalize();

		if (!loader_.Initialize(filePath))
		{
			return false;
		}

		textureWidth_ = loader_.GetWidth();
		textureHeight_ = loader_.GetHeight();

		playbackTime_ = 0.0;
		hasPendingFrame_ = false;
		hasLastUpdateTime_ = false;
		autoPlayStarted_ = false;

		return true;
	}

	void MovieResource::Entry::Finalize()
	{
		loader_.Finalize();

		frameTexture_.Reset();
		uploadBuffers_[0].Reset();
		uploadBuffers_[1].Reset();
		alignedRowPitch_ = 0;
		uploadBufferParity_ = 0;
		frameTextureIndexAllocated_ = false;
		frameDirty_ = false;

		textureWidth_ = 0;
		textureHeight_ = 0;

		playbackTime_ = 0.0;
		pendingFrameTime_ = 0.0;
		hasPendingFrame_ = false;
		playing_ = false;
		autoPlayStarted_ = false;
		hasLastUpdateTime_ = false;
	}

	void MovieResource::Entry::Update()
	{
		auto now = std::chrono::steady_clock::now();

		if (!hasLastUpdateTime_)
		{
			lastUpdateTime_ = now;
			hasLastUpdateTime_ = true;
			return;
		}

		Double elapsed = std::chrono::duration<Double>(now - lastUpdateTime_).count();
		lastUpdateTime_ = now;

		if (!playing_)
		{
			return;
		}

		playbackTime_ += elapsed;

		if (!hasPendingFrame_)
		{
			Double timestamp = 0.0;
			if (loader_.ReadNextSample(timestamp))
			{
				pendingFrameTime_ = timestamp;
				hasPendingFrame_ = true;
			}
		}

		while (hasPendingFrame_ && pendingFrameTime_ <= playbackTime_)
		{
			frameDirty_ = true;

			Double timestamp = 0.0;
			if (loader_.ReadNextSample(timestamp))
			{
				pendingFrameTime_ = timestamp;
				hasPendingFrame_ = true;
			}
			else
			{
				hasPendingFrame_ = false;
			}
		}

		if (loader_.IsEndOfStream())
		{
			if (loop_)
			{
				loader_.Seek(0.0);
				playbackTime_ = 0.0;
				hasPendingFrame_ = false;
			}
			else
			{
				playing_ = false;
			}
		}
	}

	void MovieResource::Entry::UploadFrame(ID3D12Device* device, ID3D12GraphicsCommandList6* cmdList, BindlessHeap* bindlessHeap)
	{
		if (!frameDirty_)
		{
			return;
		}

		if (textureWidth_ <= 0 || textureHeight_ <= 0)
		{
			return;
		}

		if (!frameTextureIndexAllocated_)
		{
			frameTextureIndex_ = bindlessHeap->AllocateIndex();
			frameTextureIndexAllocated_ = true;
		}

		Longlong sourceRowPitch = loader_.GetRowPitch();
		alignedRowPitch_ = (sourceRowPitch + (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) & ~static_cast<Longlong>(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

		if (!frameTexture_)
		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Width = static_cast<Uint64>(textureWidth_);
			resourceDesc.Height = static_cast<Uint>(textureHeight_);
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

			if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(frameTexture_.ReleaseAndGetAddressOf()))))
			{
				SC_LOG_ERROR("MovieResource: フレームテクスチャの生成に失敗しました (width={}, height={})", textureWidth_, textureHeight_);
				return;
			}

			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(frameTexture_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(frameTextureIndex_));

			D3D12_HEAP_PROPERTIES uploadHeapProperties{};
			uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

			D3D12_RESOURCE_DESC uploadResourceDesc{};
			uploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			uploadResourceDesc.Width = static_cast<Uint64>(alignedRowPitch_ * textureHeight_);
			uploadResourceDesc.Height = 1;
			uploadResourceDesc.DepthOrArraySize = 1;
			uploadResourceDesc.MipLevels = 1;
			uploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			uploadResourceDesc.SampleDesc.Count = 1;
			uploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			for (Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer : uploadBuffers_)
			{
				if (FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf()))))
				{
					return;
				}
			}
		}
		else
		{
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = frameTexture_.Get();
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			cmdList->ResourceBarrier(1, &barrier);
		}

		ID3D12Resource* uploadBuffer = uploadBuffers_[uploadBufferParity_].Get();
		uploadBufferParity_ = (uploadBufferParity_ + 1) % 2;

		const Byte* sourcePixels = loader_.GetPixelData();

		Byte* mappedData = nullptr;
		if (FAILED(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData))))
		{
			return;
		}

		for (Int row = 0; row < textureHeight_; ++row)
		{
			std::memcpy(mappedData + static_cast<Size>(row) * alignedRowPitch_, sourcePixels + static_cast<Size>(row) * sourceRowPitch, static_cast<Size>(sourceRowPitch));
		}

		uploadBuffer->Unmap(0, nullptr);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
		footprint.Offset = 0;
		footprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		footprint.Footprint.Width = static_cast<Uint>(textureWidth_);
		footprint.Footprint.Height = static_cast<Uint>(textureHeight_);
		footprint.Footprint.Depth = 1;
		footprint.Footprint.RowPitch = static_cast<Uint>(alignedRowPitch_);

		D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
		destinationLocation.pResource = frameTexture_.Get();
		destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		destinationLocation.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
		sourceLocation.pResource = uploadBuffer;
		sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		sourceLocation.PlacedFootprint = footprint;

		cmdList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = frameTexture_.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		cmdList->ResourceBarrier(1, &barrier);

		frameDirty_ = false;
	}

	void MovieResource::Entry::Play()
	{
		playing_ = true;
	}

	void MovieResource::Entry::Pause()
	{
		playing_ = false;
	}

	void MovieResource::Entry::Stop()
	{
		playing_ = false;
		loader_.Seek(0.0);
		playbackTime_ = 0.0;
		hasPendingFrame_ = false;
	}

	void MovieResource::Entry::SetLoop(Bool loop)
	{
		loop_ = loop;
	}

	Bool MovieResource::Entry::IsLoop()const
	{
		return loop_;
	}

	Bool MovieResource::Entry::IsPlaying()const
	{
		return playing_;
	}

	Bool MovieResource::Entry::HasAutoPlayStarted()const
	{
		return autoPlayStarted_;
	}

	void MovieResource::Entry::MarkAutoPlayStarted()
	{
		autoPlayStarted_ = true;
	}

	Uint MovieResource::Entry::GetTextureIndex()const
	{
		return frameTextureIndex_;
	}

	Bool MovieResource::Entry::HasTexture()const
	{
		return frameTexture_ != nullptr;
	}

	Int MovieResource::Entry::GetWidth()const
	{
		return textureWidth_;
	}

	Int MovieResource::Entry::GetHeight()const
	{
		return textureHeight_;
	}

	Double MovieResource::Entry::GetDuration()const
	{
		return loader_.GetDuration();
	}

	Double MovieResource::Entry::GetPlaybackTime()const
	{
		return playbackTime_;
	}

	MovieResource::MovieResource()
	{
		HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ownsComInitialize_ = (comResult == S_OK || comResult == S_FALSE);

		HRESULT mfResult = MFStartup(MF_VERSION, MFSTARTUP_LITE);
		mfStarted_ = SUCCEEDED(mfResult);

		if (!mfStarted_)
		{
			SC_LOG_WARNING("MovieResource: MFStartupに失敗しました - Movie機能は無効です");
		}
	}

	MovieResource::~MovieResource()
	{
		Clear();

		if (mfStarted_)
		{
			MFShutdown();
		}

		if (ownsComInitialize_)
		{
			CoUninitialize();
		}
	}

	Bool MovieResource::Load(Uint32 assetID, const std::string& filePath)
	{
		if (Contains(assetID))
		{
			return true;
		}

		ResourcePtr<Entry> entry = MakePtr<Entry>();
		if (!entry->Initialize(filePath))
		{
			SC_LOG_ERROR("MovieResource: 動画の読み込みに失敗しました: {}", filePath);
			return false;
		}

		movies_.insert({ assetID, std::move(entry) });

		return true;
	}

	Bool MovieResource::Contains(Uint32 assetID)const
	{
		return movies_.contains(assetID);
	}

	void MovieResource::Unload(Uint32 assetID)
	{
		if (!Contains(assetID))
		{
			return;
		}

		movies_.at(assetID)->Finalize();
		movies_.erase(assetID);
	}

	void MovieResource::AdvancePlayback(Uint32 assetID)
	{
		if (Entry* entry = Find(assetID))
		{
			entry->Update();
		}
	}

	void MovieResource::Play(Uint32 assetID)
	{
		if (Entry* entry = Find(assetID))
		{
			entry->Play();
		}
	}

	void MovieResource::Pause(Uint32 assetID)
	{
		if (Entry* entry = Find(assetID))
		{
			entry->Pause();
		}
	}

	void MovieResource::Stop(Uint32 assetID)
	{
		if (Entry* entry = Find(assetID))
		{
			entry->Stop();
		}
	}

	void MovieResource::SetLoop(Uint32 assetID, Bool loop)
	{
		if (Entry* entry = Find(assetID))
		{
			entry->SetLoop(loop);
		}
	}

	Bool MovieResource::IsPlaying(Uint32 assetID)const
	{
		if (const Entry* entry = Find(assetID))
		{
			return entry->IsPlaying();
		}
		return false;
	}

	Bool MovieResource::HasAutoPlayStarted(Uint32 assetID)const
	{
		if (const Entry* entry = Find(assetID))
		{
			return entry->HasAutoPlayStarted();
		}
		return false;
	}

	void MovieResource::MarkAutoPlayStarted(Uint32 assetID)
	{
		if (Entry* entry = Find(assetID))
		{
			entry->MarkAutoPlayStarted();
		}
	}

	Uint MovieResource::GetTextureIndex(Uint32 assetID)const
	{
		if (const Entry* entry = Find(assetID))
		{
			return entry->GetTextureIndex();
		}
		return 0;
	}

	Bool MovieResource::HasTexture(Uint32 assetID)const
	{
		if (const Entry* entry = Find(assetID))
		{
			return entry->HasTexture();
		}
		return false;
	}

	Int MovieResource::GetWidth(Uint32 assetID)const
	{
		if (const Entry* entry = Find(assetID))
		{
			return entry->GetWidth();
		}
		return 0;
	}

	Int MovieResource::GetHeight(Uint32 assetID)const
	{
		if (const Entry* entry = Find(assetID))
		{
			return entry->GetHeight();
		}
		return 0;
	}

	Double MovieResource::GetDuration(Uint32 assetID)const
	{
		if (const Entry* entry = Find(assetID))
		{
			return entry->GetDuration();
		}
		return 0.0;
	}

	Double MovieResource::GetPlaybackTime(Uint32 assetID)const
	{
		if (const Entry* entry = Find(assetID))
		{
			return entry->GetPlaybackTime();
		}
		return 0.0;
	}

	void MovieResource::Update(ID3D12Device* device, ID3D12GraphicsCommandList6* cmdList, BindlessHeap* bindlessHeap)
	{
		for (auto& entry : movies_ | std::ranges::views::values)
		{
			entry->UploadFrame(device, cmdList, bindlessHeap);
		}
	}

	void MovieResource::Clear()
	{
		for (auto& entry : movies_ | std::ranges::views::values)
		{
			entry->Finalize();
		}
		movies_.clear();
	}

	MovieResource::Entry* MovieResource::Find(Uint32 assetID)
	{
		if (!movies_.contains(assetID))
		{
			return nullptr;
		}
		return movies_.at(assetID).get();
	}

	const MovieResource::Entry* MovieResource::Find(Uint32 assetID)const
	{
		if (!movies_.contains(assetID))
		{
			return nullptr;
		}
		return movies_.at(assetID).get();
	}
}
