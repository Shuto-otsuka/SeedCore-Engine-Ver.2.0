#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/Movie/MovieDecoder.h>

namespace SeedCore
{
	class BindlessHeap;

	/**
	* [EN]
	* One loaded movie asset: the Media Foundation decoder, the frame
	* texture it uploads into, and the playback state driven by MovieSystem.
	* Opened and torn down by MovieLoader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 読み込み済みのムービーアセット1つ: Media Foundation のデコーダ、
	* フレームをアップロードするテクスチャ、および MovieSystem が駆動する
	* 再生状態。MovieLoader が開いて破棄する。
	*/
	class Video :public NonCopyable
	{
		friend class MovieLoader;

	public:
		Video() = default;
		~Video() = default;

		Video(Video&&)noexcept = default;
		Video& operator=(Video&&)noexcept = default;

	public:
		void Update();

		void UploadFrame(ID3D12Device* device, ID3D12GraphicsCommandList6* cmdList, BindlessHeap* bindlessHeap);

		void Play();

		void Pause();

		void Stop();

		void SetLoop(Bool loop);

		void MarkAutoPlayStarted();

	public:
		[[nodiscard]] Bool IsLoop()const;

		[[nodiscard]] Bool IsPlaying()const;

		[[nodiscard]] Bool HasAutoPlayStarted()const;

		[[nodiscard]] Uint GetTextureIndex()const;

		[[nodiscard]] Bool HasTexture()const;

		[[nodiscard]] Int GetWidth()const;

		[[nodiscard]] Int GetHeight()const;

		[[nodiscard]] Double GetDuration()const;

		[[nodiscard]] Double GetPlaybackTime()const;

	private:
		MovieDecoder decoder_;

		Microsoft::WRL::ComPtr<ID3D12Resource> frameTexture_;

		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffers_[2];

		Longlong alignedRowPitch_ = 0;

		Uint uploadBufferParity_ = 0;

		Uint frameTextureIndex_ = 0;

		Bool frameTextureIndexAllocated_ = false;

		Bool frameDirty_ = false;

		Int textureWidth_ = 0;

		Int textureHeight_ = 0;

		Double playbackTime_ = 0.0;

		Double pendingFrameTime_ = 0.0;

		Bool hasPendingFrame_ = false;

		Bool playing_ = false;

		Bool loop_ = true;

		Bool autoPlayStarted_ = false;

		std::chrono::steady_clock::time_point lastUpdateTime_;

		Bool hasLastUpdateTime_ = false;
	};
}
