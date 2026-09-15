#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/VolumetricLight/VolumetricLightShader.h>
#include <GraphicsEngine/Raytracing/RaytracingView.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class ConstantIndicesSystem;
	class ShaderResourceIndicesSystem;
	class UnorderedAccessIndicesSystem;
	struct RootAddresses;

	/// [EN] Mirrors Raytracing/VolumetricLight/VolumetricLight.hlsli's
	///      VolumetricLightRayConstantBuffer — read by the three froxel passes
	///      and DeferredLightingPS.hlsl via
	///      constant_indices.volumetric_light_index_. Must stay byte-for-byte
	///      in sync with the HLSL side.
	/// [JP] Raytracing/VolumetricLight/VolumetricLight.hlsli の
	///      VolumetricLightRayConstantBuffer と対応。froxel 3パスと
	///      DeferredLightingPS.hlsl が constant_indices.volumetric_light_index_
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
		void Serialize(Archive& archive)
		{
			archive.TryField("density", density_);
			archive.TryField("absorption", absorption_);
			archive.TryField("heightFalloff", heightFalloff_);
			archive.TryField("heightReference", heightReference_);
			archive.TryField("fogAlbedo", fogAlbedo_);
			archive.TryField("scatteringG", scatteringG_);
			archive.TryField("rayTMax", rayTMax_);
			archive.TryField("godrayStrength", godrayStrength_);
			archive.TryField("cloudShadowEnabled", cloudShadowEnabled_);
		}
	};

	/**
	* [EN]
	* Runs the froxel volumetric pipeline: FogInjectionCS (medium) ->
	* VolumetricLightScatteringRT (sun occlusion via inline RayQuery + cloud
	* lightmarch = god rays) -> FroxelIntegrationCS (front-to-back scan into
	* the integration volume, sampled by DeferredLightingPS.hlsl with a
	* linear sampler at each pixel's depth slice). The scattering volume is
	* jittered every frame and temporally accumulated, so each view owns its
	* own history/write ping-pong pair; the density and integration volumes
	* are shared and re-written per flush (160x90x128 grid).
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
	* 散乱ボリュームは毎フレームジッターして時間積分するので、ビューごとに
	* history/write のピンポンを持つ。密度と積分ボリュームは共有で Flush ごとに
	* 書き直す(160x90x128 のグリッド)。無効時は積分ボリュームを (0,0,0,1)=
	* 散乱なし・全透過にクリアするので合成は実質no-op。
	*/
	class VolumetricLightRenderer
	{
	public:
		static constexpr Uint32 froxelDimensionX = 160;
		static constexpr Uint32 froxelDimensionY = 90;
		static constexpr Uint32 froxelDimensionZ = 128;

		VolumetricLightRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~VolumetricLightRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, ConstantIndicesSystem& constantIndicesSystem, ShaderResourceIndicesSystem& shaderResourceIndicesSystem, UnorderedAccessIndicesSystem& unorderedAccessIndicesSystem, Uint32 width, Uint32 height);

		/// [EN] Updates the tuning constant buffer (stamping the froxel
		///      dimensions) and registers every bindless index into
		///      the index systems. Must run before the index systems' UploadEditor/
		///      UploadGame bakes this frame's indices. No GPU work.
		/// [JP] チューニング用定数バッファを更新し(froxel 次元を焼き込む)、
		///      bindless インデックスを 各インデックスシステムへ登録する。
		///      各インデックスシステムの UploadEditor/UploadGame が今フレームの
		///      インデックスを確定する前に呼ぶこと。GPU 処理は無い。
		void PrepareFrame(const VolumetricLightRayConstantBuffer& settings);

		/// [EN] The actual GPU work: the three froxel dispatches with UAV
		///      barriers between them (or an integration-volume clear to
		///      (0,0,0,1) when disabled / PSOs missing), leaving the
		///      integration volume in PIXEL_SHADER_RESOURCE state.
		/// [JP] 実際の GPU 処理: UAV バリアを挟んだ froxel 3ディスパッチ
		///      (無効時/PSO 無し時は積分ボリュームを (0,0,0,1) にクリア)。
		///      積分ボリュームは PIXEL_SHADER_RESOURCE 状態で終える。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses, RaytracingView view, Bool enabled);

	private:
		static constexpr Uint32 scatteringSlotCount_ = 2;
		static constexpr Uint32 froxelJitterSequenceLength_ = 16;

		struct VolumetricLightDispatchConstantBuffer
		{
			Uint scatteringWriteUnorderedAccessViewIndex_ = 0;
			Uint scatteringHistoryShaderResourceViewIndex_ = 0;
			Uint historyValid_ = 0;
			Uint frameIndex_ = 0;

			Vector3 jitter_ = { 0.0f, 0.0f, 0.0f };
			Float volumetricLightDispatchPadding_ = 0.0f;
		};

		struct View
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> scatteringVolumeResource_[scatteringSlotCount_];
			D3D12_RESOURCE_STATES scatteringVolumeState_[scatteringSlotCount_] = { D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON };
			Uint32 scatteringVolumeUnorderedAccessViewIndex_[scatteringSlotCount_] = { 0, 0 };
			Uint32 scatteringVolumeShaderResourceViewIndex_[scatteringSlotCount_] = { 0, 0 };
			Uint32 writeSlot_ = 0;
			Uint32 frameIndex_ = 0;
			Bool historyValid_ = false;

			ResourcePtr<ConstantBuffer<VolumetricLightDispatchConstantBuffer>> constantBuffer_;
		};

		[[nodiscard]] View& ViewFor(RaytracingView view);

		void CreateVolume(ID3D12Device* device, BindlessHeap* bindlessHeap, Bool createShaderResourceView,
			Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32* outShaderResourceViewIndex, Uint32* outClearIndex);

		VolumetricLightShader volumetricLightShader_;

		ResourcePtr<ConstantBuffer<VolumetricLightRayConstantBuffer>> tuningBuffer_;

		/// [EN] Pass 1 output (rgb = scattering coefficient, a = extinction).
		/// [JP] パス1出力(rgb=散乱係数、a=消衰)。
		Microsoft::WRL::ComPtr<ID3D12Resource> densityVolumeResource_;
		Uint32 densityVolumeUnorderedAccessViewIndex_ = 0;

		/// [EN] Pass 2 output (rgb = in-scattered light, a = extinction), one
		///      history/write pair per view.
		/// [JP] パス2出力(rgb=内散乱、a=消衰)。ビューごとに history/write の組。
		View editorView_;
		View gameView_;

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

		/// [EN] The density volume stays in UNORDERED_ACCESS for its whole life
		///      (written+read by UAV only); transitioned once.
		/// [JP] density ボリュームは生涯 UNORDERED_ACCESS のまま
		///      (UAV でしか読み書きしない)。初回に一度だけ遷移する。
		Bool workingVolumesTransitioned_ = false;

		BindlessHeap* bindlessHeap_ = nullptr;
		ConstantIndicesSystem* constantIndicesSystem_ = nullptr;
		ShaderResourceIndicesSystem* shaderResourceIndicesSystem_ = nullptr;
		UnorderedAccessIndicesSystem* unorderedAccessIndicesSystem_ = nullptr;

		/// [EN] Logs the PSO-creation-failed warning once instead of every frame.
		/// [JP] PSO 作成失敗の警告を毎フレームでなく 1 度だけログ出力する。
		Bool pipelineStateMissingLogged_ = false;
	};
}
