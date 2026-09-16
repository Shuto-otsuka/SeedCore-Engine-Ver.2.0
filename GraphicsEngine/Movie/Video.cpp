#include <GraphicsEngine/Movie/Video.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	void Video::Update()
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
			if (decoder_.ReadNextSample(timestamp))
			{
				pendingFrameTime_ = timestamp;
				hasPendingFrame_ = true;
			}
		}

		while (hasPendingFrame_ && pendingFrameTime_ <= playbackTime_)
		{
			frameDirty_ = true;

			Double timestamp = 0.0;
			if (decoder_.ReadNextSample(timestamp))
			{
				pendingFrameTime_ = timestamp;
				hasPendingFrame_ = true;
			}
			else
			{
				hasPendingFrame_ = false;
			}
		}

		if (decoder_.IsEndOfStream())
		{
			if (loop_)
			{
				decoder_.Seek(0.0);
				playbackTime_ = 0.0;
				hasPendingFrame_ = false;
			}
			else
			{
				playing_ = false;
			}
		}
	}

	void Video::UploadFrame(ID3D12Device* device, ID3D12GraphicsCommandList6* cmdList, BindlessHeap* bindlessHeap)
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

		Longlong sourceRowPitch = decoder_.GetRowPitch();
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
#ifdef _DEBUG
			frameTexture_->SetName(L"Movie_FrameTexture");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(frameTexture_.Get());
#endif

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
#ifdef _DEBUG
				uploadBuffer->SetName(L"Movie_UploadBuffer");
				GFSDK_Aftermath_DX12_UpdateResourceInfo(uploadBuffer.Get());
#endif
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

		const Byte* sourcePixels = decoder_.GetPixelData();

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

	void Video::Play()
	{
		playing_ = true;
	}

	void Video::Pause()
	{
		playing_ = false;
	}

	void Video::Stop()
	{
		playing_ = false;
		decoder_.Seek(0.0);
		playbackTime_ = 0.0;
		hasPendingFrame_ = false;
	}

	void Video::SetLoop(Bool loop)
	{
		loop_ = loop;
	}

	Bool Video::IsLoop()const
	{
		return loop_;
	}

	Bool Video::IsPlaying()const
	{
		return playing_;
	}

	Bool Video::HasAutoPlayStarted()const
	{
		return autoPlayStarted_;
	}

	void Video::MarkAutoPlayStarted()
	{
		autoPlayStarted_ = true;
	}

	Uint Video::GetTextureIndex()const
	{
		return frameTextureIndex_;
	}

	Bool Video::HasTexture()const
	{
		return frameTexture_ != nullptr;
	}

	Int Video::GetWidth()const
	{
		return textureWidth_;
	}

	Int Video::GetHeight()const
	{
		return textureHeight_;
	}

	Double Video::GetDuration()const
	{
		return decoder_.GetDuration();
	}

	Double Video::GetPlaybackTime()const
	{
		return playbackTime_;
	}
}
