#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Serialization/SerializeFallback.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/VolumetricLight/VolumetricLightShader.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/// [EN] Mirrors Raytracing/VolumetricLight/VolumetricLight.hlsli's
	///      VolumetricLightRayConstantBuffer — read by the three froxel passes
	///      and DeferredLightingPS.hlsl via
	///      structured_indices.vl_ray_constant_index_. Must stay byte-for-byte
	///      in sync with the HLSL side.
	/// [JP] Raytracing/VolumetricLight/VolumetricLight.hlsli の
	///      VolumetricLightRayConstantBuffer と対応。froxel 3パスと
	///      DeferredLightingPS.hlsl が structured_indices.vl_ray_constant_index_
	///      経由で読む。HLSL 側とバイト単位で一致させること。
	struct VolumetricLightRayConstantBuffer
	{
		/// [EN] Base fog density (scattering strength).
		/// [JP] ベースのフォグ密度(散乱の強さ)。
		/// [JP] 注意: far plane までの光路長×消衰が光学的深さになるので、
		///      0.01 でも「数百 unit 先が見えない濃霧」。既定はうっすら。
		Float density_ = 0.003f;

		/// [EN] Absorption coefficient (extinction = density + absorption).
		/// [JP] 吸収係数(消衰 = 密度 + 吸収)。
		Float absorption_ = 0.0005f;

		/// [EN] Height-fog falloff (0 = uniform fog).
		/// [JP] 高さフォグの減衰(0 で一様フォグ)。既定は地表近くに溜まる霧。
		Float heightFalloff_ = 0.05f;

		/// [EN] Height-fog reference Y.
		/// [JP] 高さフォグの基準 Y。
		Float heightReference_ = 0.0f;

		Float fogAlbedo_[3] = { 1.0f, 1.0f, 1.0f };

		/// [EN] Henyey-Greenstein anisotropy (higher = stronger god rays).
		/// [JP] Henyey-Greenstein の異方性(高いほどゴッドレイが強く出る)。
		Float scatteringG_ = 0.7f;

		/// [EN] Max shadow-ray length for the froxel sun-occlusion test.
		/// [JP] froxel の太陽遮蔽テストのシャドウレイ最大長。
		Float rayTMax_ = 2000.0f;

		/// [EN] Multiplier on the sun in-scattering term.
		/// [JP] 太陽の内散乱項の倍率(ゴッドレイの強さ)。
		Float godrayStrength_ = 1.0f;

		/// [EN] Froxel grid dimensions — set by VolumetricLightRenderer (not
		///      the UI).
		/// [JP] froxel グリッドの次元 — VolumetricLightRenderer が設定する
		///      (UI からは触らない)。
		Uint32 froxelDimensionX_ = 0;
		Uint32 froxelDimensionY_ = 0;
		Uint32 froxelDimensionZ_ = 0;

		/// [EN] 1 = attenuate the sun by a short cloud lightmarch (crepuscular
		///      rays through cloud gaps; needs the procedural cloud system).
		/// [JP] 1=太陽を短い雲ライトマーチで減光する(雲間からの光芒。
		///      プロシージャル雲システムが必要)。
		Uint32 cloudShadowEnabled_ = 1;

		Float volumetricLightPadding_[2] = { 0.0f, 0.0f };

		template<class Archive>
		void serialize(Archive& archive)
		{
			TryLoadField(archive, "density", density_);
			TryLoadField(archive, "absorption", absorption_);
			TryLoadField(archive, "heightFalloff", heightFalloff_);
			TryLoadField(archive, "heightReference", heightReference_);
			TryLoadField(archive, "fogAlbedo", fogAlbedo_);
			TryLoadField(archive, "scatteringG", scatteringG_);
			TryLoadField(archive, "rayTMax", rayTMax_);
			TryLoadField(archive, "godrayStrength", godrayStrength_);
			TryLoadField(archive, "cloudShadowEnabled", cloudShadowEnabled_);
		}
	};

	/**
	* [EN]
	* Runs the froxel volumetric pipeline: FogInjectionCS (medium) ->
	* VolumetricLightScatteringRT (sun occlusion via inline RayQuery + cloud
	* lightmarch = god rays) -> FroxelIntegrationCS (front-to-back scan into
	* the integration volume, sampled by DeferredLightingPS.hlsl with a
	* linear sampler at each pixel's depth slice). Deterministic — no
	* denoiser, no per-view chain (one 160x90x64 grid re-written per flush).
	* When disabled the integration volume is cleared to (0,0,0,1) =
	* no scattering, full transmittance, so the composite is a no-op.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Froxel ボリューメトリクスパイプラインを実行する: FogInjectionCS(媒質)→
	* VolumetricLightScatteringRT(インライン RayQuery の太陽遮蔽+雲ライト
	* マーチ=ゴッドレイ)→ FroxelIntegrationCS(front-to-back 積分。
	* DeferredLightingPS.hlsl が各ピクセルの深度スライスでリニアサンプル)。
	* 決定論的 — デノイザもビュー別チェーンも無し(160x90x64 のグリッド1式を
	* Flush ごとに書き直す)。無効時は積分ボリュームを (0,0,0,1)=散乱なし・
	* 全透過にクリアするので合成は実質no-op。
	*/
	class VolumetricLightRenderer
	{
	public:
		static constexpr Uint32 froxelDimensionX = 160;
		static constexpr Uint32 froxelDimensionY = 90;
		static constexpr Uint32 froxelDimensionZ = 64;

		VolumetricLightRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~VolumetricLightRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		/// [EN] Updates the tuning constant buffer (stamping the froxel
		///      dimensions) and registers every bindless index into
		///      IndicesSystem. Must run before IndicesSystem::UploadEditor/
		///      UploadGame bakes this frame's indices. No GPU work.
		/// [JP] チューニング用定数バッファを更新し(froxel 次元を焼き込む)、
		///      bindless インデックスを IndicesSystem へ登録する。
		///      IndicesSystem::UploadEditor/UploadGame が今フレームの
		///      インデックスを確定する前に呼ぶこと。GPU 処理は無い。
		void PrepareFrame(const VolumetricLightRayConstantBuffer& settings);

		/// [EN] The actual GPU work: the three froxel dispatches with UAV
		///      barriers between them (or an integration-volume clear to
		///      (0,0,0,1) when disabled / PSOs missing), leaving the
		///      integration volume in PIXEL_SHADER_RESOURCE state.
		/// [JP] 実際の GPU 処理: UAV バリアを挟んだ froxel 3ディスパッチ
		///      (無効時/PSO 無し時は積分ボリュームを (0,0,0,1) にクリア)。
		///      積分ボリュームは PIXEL_SHADER_RESOURCE 状態で終える。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool enabled);

	private:
		void CreateVolume(ID3D12Device* device, BindlessHeap* bindlessHeap, Bool createShaderResourceView,
			Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32* outShaderResourceViewIndex, Uint32* outClearIndex);

		VolumetricLightShader volumetricLightShader_;

		ResourcePtr<ConstantBuffer<VolumetricLightRayConstantBuffer>> tuningBuffer_;

		/// [EN] Pass 1 output (rgb = scattering coefficient, a = extinction).
		/// [JP] パス1出力(rgb=散乱係数、a=消衰)。
		Microsoft::WRL::ComPtr<ID3D12Resource> densityVolumeResource_;
		Uint32 densityVolumeUnorderedAccessViewIndex_ = 0;

		/// [EN] Pass 2 output (rgb = in-scattered light, a = extinction).
		/// [JP] パス2出力(rgb=内散乱、a=消衰)。
		Microsoft::WRL::ComPtr<ID3D12Resource> scatteringVolumeResource_;
		Uint32 scatteringVolumeUnorderedAccessViewIndex_ = 0;

		/// [EN] Pass 3 output (rgb = accumulated scattering, a =
		///      transmittance), sampled by the composite.
		/// [JP] パス3出力(rgb=累積散乱、a=透過率)。合成がサンプルする。
		Microsoft::WRL::ComPtr<ID3D12Resource> integrationVolumeResource_;
		D3D12_RESOURCE_STATES integrationVolumeState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 integrationVolumeUnorderedAccessViewIndex_ = 0;
		Uint32 integrationVolumeShaderResourceViewIndex_ = 0;

		/// [EN] Non-shader-visible UAV descriptor required by
		///      ClearUnorderedAccessViewFloat for the disabled-clear.
		/// [JP] 無効時クリアの ClearUnorderedAccessViewFloat が要求する
		///      非シェーダ可視 UAV ディスクリプタ。
		DescriptorHeap clearHeap_;
		Uint32 clearIntegrationIndex_ = 0;

		/// [EN] density/scattering volumes stay in UNORDERED_ACCESS for their
		///      whole life (written+read by UAV only); transitioned once.
		/// [JP] density/scattering ボリュームは生涯 UNORDERED_ACCESS のまま
		///      (UAV でしか読み書きしない)。初回に一度だけ遷移する。
		Bool workingVolumesTransitioned_ = false;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		/// [EN] Logs the PSO-creation-failed warning once instead of every frame.
		/// [JP] PSO 作成失敗の警告を毎フレームでなく 1 度だけログ出力する。
		Bool pipelineStateMissingLogged_ = false;
	};
}
