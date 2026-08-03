#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/SerializeFallback.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/VolumetricStar/VolumetricStarShader.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/// [EN] One CPU-side shooting star slot. Mirrors HLSL ShootingStarInstance
	///      (Raytracing/VolumetricStar/VolumetricStar.hlsli) byte-for-byte.
	/// [JP] CPU側の流れ星スロット1つ分。HLSL 側の ShootingStarInstance
	///      (Raytracing/VolumetricStar/VolumetricStar.hlsli)とバイト単位で一致。
	struct ShootingStarInstance
	{
		Vector3 startDirection_ = { 0.0f, 1.0f, 0.0f };
		Float progress_ = 0.0f;
		Vector3 endDirection_ = { 0.0f, 1.0f, 0.0f };
		Float brightness_ = 0.0f;
	};

	static constexpr Uint32 volumetricStarMaxShootingStars_ = 4;

	/// [EN] Mirrors Raytracing/VolumetricStar/VolumetricStar.hlsli's
	///      VolumetricStarRayConstantBuffer. Read by both VolumetricStarRT.hlsl
	///      and DeferredCompositePS.hlsl via structured_indices.star_.
	///      ray_constant_index_. Must stay byte-for-byte in sync with the HLSL
	///      side - laid out in 4-scalar (16 byte) cbuffer rows, same convention
	///      as VolumetricCloudScapesRayConstantBuffer.
	/// [JP] Raytracing/VolumetricStar/VolumetricStar.hlsli の
	///      VolumetricStarRayConstantBuffer と対応。VolumetricStarRT.hlsl と
	///      DeferredCompositePS.hlsl の両方が structured_indices.star_.
	///      ray_constant_index_ 経由で読む。HLSL 側とバイト単位で一致させること
	///      - VolumetricCloudScapesRayConstantBuffer と同じく4スカラー
	///      (16バイト)単位の cbuffer 行で並べてある。
	struct VolumetricStarRayConstantBuffer
	{
		/// [EN] Angular size (radians) of the star placement grid.
		/// [JP] 星の配置グリッドの角度サイズ(ラジアン)。
		Float cellSize_ = 0.12f;

		/// [EN] 0-1, chance a grid cell holds a star.
		/// [JP] 0-1、グリッドセルに星が出現する確率。
		Float density_ = 0.35f;

		Float brightness_ = 1.0f;
		Float twinkleSpeed_ = 2.0f;

		Float color_[3] = { 0.9f, 0.95f, 1.0f };

		/// [EN] Star size in grid-cell units. Small on purpose - real stars read
		///      as pinpoints; Glow (see glowIntensity_/glowFalloff_) is what
		///      makes them visible beyond that.
		/// [JP] 星のサイズ(グリッドセル単位)。実際の星は点にしか見えないので
		///      小さめが正しい - それでも見えるようにするのが Glow
		///      (glowIntensity_/glowFalloff_)の役目。
		Float sizeMin_ = 0.02f;

		Float sizeMax_ = 0.05f;
		Float shootingStarChancePerSecond_ = 0.05f;
		Float shootingStarBrightness_ = 3.0f;

		/// [EN] Angular half-width (radians) of a shooting star streak.
		/// [JP] 流れ星の筋の角度半幅(ラジアン)。
		Float shootingStarWidth_ = 0.0015f;

		Uint32 maxConcurrentShootingStars_ = volumetricStarMaxShootingStars_;

		/// [EN] Stamped by VolumetricStarRenderer (not the UI), like
		///      VolumetricCloudScapesRayConstantBuffer's proceduralSkyEnabled_.
		/// [JP] VolumetricStarRenderer が設定する(UIからは触らない)、
		///      VolumetricCloudScapesRayConstantBuffer の proceduralSkyEnabled_
		///      と同じ扱い。
		Uint32 enabled_ = 0;

		/// [EN] Strength of the soft halo outside the SDF star shape.
		/// [JP] SDF星形の外側に広がる、柔らかいハローの強さ。
		Float glowIntensity_ = 0.8f;

		/// [EN] How fast the halo fades with SDF distance - higher = tighter glow.
		/// [JP] SDF距離に対するハローの減衰速度 - 大きいほどハローが締まる。
		Float glowFalloff_ = 120.0f;

		/// [EN] Live CPU-managed shooting star slots, updated each PrepareFrame.
		/// [JP] PrepareFrame毎に更新される、CPU管理の流れ星スロット。
		ShootingStarInstance activeShootingStars_[volumetricStarMaxShootingStars_];

		template<class Archive>
		void serialize(Archive& archive)
		{
			TryLoadField(archive, "cellSize", cellSize_);
			TryLoadField(archive, "density", density_);
			TryLoadField(archive, "brightness", brightness_);
			TryLoadField(archive, "twinkleSpeed", twinkleSpeed_);
			TryLoadField(archive, "color", color_);
			TryLoadField(archive, "sizeMin", sizeMin_);
			TryLoadField(archive, "sizeMax", sizeMax_);
			TryLoadField(archive, "shootingStarChancePerSecond", shootingStarChancePerSecond_);
			TryLoadField(archive, "shootingStarBrightness", shootingStarBrightness_);
			TryLoadField(archive, "shootingStarWidth", shootingStarWidth_);
			TryLoadField(archive, "glowIntensity", glowIntensity_);
			TryLoadField(archive, "glowFalloff", glowFalloff_);
		}
	};

	/// [EN] The HLSL mirror is 12 cbuffer rows of 4 scalars (4 tuning rows +
	///      4 shooting star slots x 2 rows). Nothing catches a layout drift at
	///      runtime, so assert the size here.
	/// [JP] HLSL 側は 4 スカラー × 12 行(チューニング4行 + 流れ星スロット4個 ×
	///      2行)。レイアウトのずれは実行時に検出できないため、サイズをここで
	///      静的検証する。
	static_assert(sizeof(VolumetricStarRayConstantBuffer) == 12 * 4 * sizeof(Float), "VolumetricStarRayConstantBuffer が VolumetricStar.hlsli とバイト単位で一致していません");

	/**
	* [EN]
	* Dispatches the volumetric star pass (VolumetricStarRT.hlsl - screen-space,
	* sky pixels only) into an RGBA16F texture (rgb = pre-multiplied color, a =
	* coverage) and leaves it in PIXEL_SHADER_RESOURCE state for
	* DeferredCompositePS.hlsl to composite over the sky, the same way
	* VolumetricCloudScapesRenderer's cloud texture is composited. Also owns the
	* CPU-side shooting star spawn/lifetime bookkeeping (PrepareFrame), since
	* that state must persist frame to frame and the tuning cbuffer is the
	* simplest place to carry it down to the shader (no separate
	* StructuredBuffer needed for only 4 slots).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボリューメトリック・スターパス(VolumetricStarRT.hlsl - スクリーン空間、
	* 空ピクセル限定)を RGBA16F テクスチャ(rgb=事前乗算色、a=カバレッジ)へ
	* ディスパッチし、DeferredCompositePS.hlsl が空の上に合成できるよう
	* PIXEL_SHADER_RESOURCE 状態にしておく
	* (VolumetricCloudScapesRenderer の雲テクスチャと同じ合成方式)。流れ星の
	* CPU側スポーン/寿命管理(PrepareFrame)もここが持つ - フレームを跨いで
	* 状態を保持する必要があり、チューニング cbuffer がシェーダへ運ぶ
	* 最も単純な経路のため(4スロットのみなので専用 StructuredBuffer は不要)。
	*/
	class VolumetricStarRenderer
	{
	public:
		VolumetricStarRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~VolumetricStarRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Advances shooting star slots (spawn roll, progress, expiry),
		///      updates the tuning constant buffer (stamping enabled into
		///      enabled_) and registers its bindless indices into IndicesSystem.
		///      Must run before IndicesSystem::UploadEditor/UploadGame bakes this
		///      frame's indices. No GPU work.
		/// [JP] 流れ星スロットを進め(スポーン抽選・進行・満了)、チューニング用
		///      定数バッファを更新し(enabled を enabled_ に焼き込む)、bindless
		///      インデックスを IndicesSystem へ登録する。IndicesSystem::
		///      UploadEditor/UploadGame が今フレームのインデックスを確定する前に
		///      呼ぶこと。GPU 処理は無い。
		void PrepareFrame(const VolumetricStarRayConstantBuffer& settings, Bool enabled, Float deltaTime, Float nightFactor);

		/// [EN] The actual GPU work: dispatches the pass (or clears the texture
		///      to 0 if enabled is false or the PSO is missing), leaving it in
		///      PIXEL_SHADER_RESOURCE state. Requires the G-Buffer depth to
		///      already be written (sky-pixel test).
		/// [JP] 実際の GPU 処理: パスをディスパッチする(enabled が false か PSO が
		///      無ければ 0 でクリア)。テクスチャは PIXEL_SHADER_RESOURCE 状態で
		///      終える。G-Buffer の深度が書き込み済みであることが前提
		///      (空ピクセル判定)。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool enabled);

	private:
		void SpawnShootingStar(Uint32 slot);

		Float RandomRange(Float minValue, Float maxValue);

	private:
		VolumetricStarShader starShader_;

		std::mt19937 randomEngine_ = std::mt19937(std::random_device{}());

		ResourcePtr<ConstantBuffer<VolumetricStarRayConstantBuffer>> tuningBuffer_;

		ShootingStarInstance shootingStars_[volumetricStarMaxShootingStars_];
		Float shootingStarDuration_[volumetricStarMaxShootingStars_] = {};

		Microsoft::WRL::ComPtr<ID3D12Resource> starResource_;
		D3D12_RESOURCE_STATES starState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 starUnorderedAccessViewIndex_ = 0;
		Uint32 starShaderResourceViewIndex_ = 0;

		DescriptorHeap clearHeap_;
		Uint32 clearIndex_ = 0;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Uint32 width_ = 0;
		Uint32 height_ = 0;

		Bool pipelineStateMissingLogged_ = false;
	};
}
