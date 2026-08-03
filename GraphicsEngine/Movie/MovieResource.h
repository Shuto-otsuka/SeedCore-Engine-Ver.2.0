#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <GraphicsEngine/Movie/MovieLoader.h>

namespace SeedCore
{
	class BindlessHeap;

	class MovieResource :public NonCopyable
	{
	private:
		class Entry
		{
		public:
			Bool Initialize(const std::string& filePath);

			void Finalize();

			void Update();

			void UploadFrame(ID3D12Device* device, ID3D12GraphicsCommandList6* cmdList, BindlessHeap* bindlessHeap);

			void Play();

			void Pause();

			void Stop();

			void SetLoop(Bool loop);

			[[nodiscard]] Bool IsLoop()const;

			[[nodiscard]] Bool IsPlaying()const;

			[[nodiscard]] Bool HasAutoPlayStarted()const;

			void MarkAutoPlayStarted();

			[[nodiscard]] Uint GetTextureIndex()const;

			[[nodiscard]] Bool HasTexture()const;

			[[nodiscard]] Int GetWidth()const;

			[[nodiscard]] Int GetHeight()const;

			[[nodiscard]] Double GetDuration()const;

			[[nodiscard]] Double GetPlaybackTime()const;

		private:
			MovieLoader loader_;

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

	public:
		MovieResource();

		~MovieResource();

		Bool Load(Uint32 assetID, const std::string& filePath);

		[[nodiscard]] Bool Contains(Uint32 assetID)const;

		void Unload(Uint32 assetID);

		void AdvancePlayback(Uint32 assetID);

		void Play(Uint32 assetID);

		void Pause(Uint32 assetID);

		void Stop(Uint32 assetID);

		void SetLoop(Uint32 assetID, Bool loop);

		[[nodiscard]] Bool IsPlaying(Uint32 assetID)const;

		[[nodiscard]] Bool HasAutoPlayStarted(Uint32 assetID)const;

		void MarkAutoPlayStarted(Uint32 assetID);

		[[nodiscard]] Uint GetTextureIndex(Uint32 assetID)const;

		[[nodiscard]] Bool HasTexture(Uint32 assetID)const;

		[[nodiscard]] Int GetWidth(Uint32 assetID)const;

		[[nodiscard]] Int GetHeight(Uint32 assetID)const;

		[[nodiscard]] Double GetDuration(Uint32 assetID)const;

		[[nodiscard]] Double GetPlaybackTime(Uint32 assetID)const;

		void Update(ID3D12Device* device, ID3D12GraphicsCommandList6* cmdList, BindlessHeap* bindlessHeap);

		void Clear();

	private:
		[[nodiscard]] Entry* Find(Uint32 assetID);

		[[nodiscard]] const Entry* Find(Uint32 assetID)const;

		FlatMap<Uint32, ResourcePtr<Entry>> movies_;

		Bool ownsComInitialize_ = false;

		Bool mfStarted_ = false;
	};
}
