#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/System/SceneSystem.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <GraphicsEngine/Light/SkyLight.h>

namespace SeedCore
{
	class BindlessHeap;
	class World;
	class ShaderCache;
	class RootSignature;
	class PipelineStateObject;
	class ComputeShader;
	class D3D12CommandList;
	class ResourceCache;
	struct LoaderSystem;

	/**
	* [EN]
	* Per-dispatch constants for the skymap IBL generation compute passes.
	* Mirrors the HLSL SkyGenerateConstant (Sky/SkyGenerate.hlsli); 32 bytes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* IBL 生成コンピュートパスのディスパッチ毎定数。HLSL の SkyGenerateConstant
	* （Sky/SkyGenerate.hlsli）と一致。32 バイト。
	*/
	struct SkyGenerateConstant
	{
		Uint sourceIndex_ = 0;
		Uint destIndex_ = 0;
		Uint faceSize_ = 0;
		Uint sampleCount_ = 0;
		Float roughness_ = 0.0f;
		Uint mipLevel_ = 0;
		Uint faceOffset_ = 0;
		Uint skyGeneratePadding0_ = 0;
	};

	/**
	* [EN]
	* Owns the whole image-based-lighting machinery for the scene's sky and
	* drives it from a pluggable sky source. Holds the environment cube (sky),
	* the dynamic cube (sky + captured scene, Realtime only), the diffuse
	* irradiance / specular prefiltered cubes and the shared BRDF lookup table,
	* plus every compute / capture pass.
	*
	* Sky sources:
	*   - Lietime (static): an HDR skymap fills the environment cube once.
	*   - Realtime (dynamic): each interval one dynamic-cube face is refreshed
	*     with the sky (currently the HDR skymap; later a volumetric-cloud pass
	*     that needs no skymap) and the opaque scene, then re-convolved. The
	*     environment cube stays sky-only so the skybox background never shows
	*     scene geometry; only the reflections (IBL) pick up the dynamic scene.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シーンの空の IBL 機構一式を所有し、差し替え可能な空ソースから駆動する。
	* environment キューブ（空）、dynamic キューブ（空 + キャプチャしたシーン。
	* Realtime のみ）、拡散 irradiance / 鏡面 prefilter キューブ、共有 BRDF
	* ルックアップテーブル、および全コンピュート / キャプチャパスを持つ。
	*
	* 空ソース:
	*   - Lietime（静的）: HDR スカイマップが environment キューブを 1 回充填。
	*   - Realtime（動的）: 一定間隔で dynamic キューブの 1 面を空（現状は HDR
	*     スカイマップ。将来はスカイマップ不要のボリューメトリッククラウドパス）
	*     と不透明シーンで更新し、再畳み込みする。environment キューブは空だけの
	*     まま保つのでスカイボックス背景にシーンは出ず、反射（IBL）だけが動的
	*     シーンを拾う。
	*/
	class SkyRenderer :public NonCopyable
	{
	public:
		SkyRenderer() = default;
		~SkyRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);

		void Gather(LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world);

		void SetIndices(IndicesSystem& indicesSystem);

		/// [EN] Procedural-sky IBL mode (used only when no skymap is bound):
		///      renders VolumetricCloudScapes' analytic sky+sun into the
		///      environment cube and convolves it, so the procedural sky
		///      lights the scene. settingsHash triggers a regenerate when the
		///      sky parameters change; lightIndex is the LightConstantData
		///      bindless index (sun direction).
		/// [JP] プロシージャル空の IBL モード(スカイマップ未バインド時のみ):
		///      VolumetricCloudScapes の解析的な空+太陽を environment キューブへ
		///      描いて畳み込み、プロシージャル空がシーンを照らすようにする。
		///      settingsHash は空パラメータ変更時の再生成トリガー。lightIndex は
		///      LightConstantData の bindless インデックス(太陽方向用)。
		void SetProceduralSky(Bool enabled, Uint32 settingsHash, Uint lightIndex, Float totalTime);

		/// [EN] One-time BRDF LUT + static environment generation (Lietime, or the
		///      base sky for Realtime) when the sky source changes. Also drives the
		///      procedural-sky regenerate; structuredAddress is needed by that pass
		///      (root parameter 3) and must point at this frame's uploaded indices.
		/// [JP] BRDF LUT と、空ソース変更時の静的 environment 生成（Lietime、または
		///      Realtime のベース空）を 1 回だけ行う。プロシージャル空の再生成も
		///      ここで駆動する。structuredAddress はそのパスが使う(ルート
		///      パラメータ3)ため、今フレームのアップロード済みインデックスを指すこと。
		void Generate(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS structuredAddress);

		[[nodiscard]] Bool RealtimeCaptureReady(Float deltaTime);

		[[nodiscard]] Uint CurrentCaptureFace()const;

		void BeginFaceCapture(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, Uint face, Uint lightIndex);

		void EndFaceCapture(D3D12CommandList* cmdList, Uint face);

		void ConvolveRealtime(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap);

		void AdvanceCaptureFace();

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS CaptureConstantAddress()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateComputePipeline(ID3D12Device* device, ShaderCache& shaderCache, PipelineStateObject& pipelineStateObject, const String& filePath);

		void CreateBrdfLookupTable(ID3D12Device* device, BindlessHeap* bindlessHeap);

		void CreateIblCubes(ID3D12Device* device, BindlessHeap* bindlessHeap);

		void CreateCube(ID3D12Device* device, BindlessHeap* heap, Uint faceSize, Uint mipLevels, DescriptorHeap* renderTargetViewHeap, D3D12_RESOURCE_STATES initialState,
			Microsoft::WRL::ComPtr<ID3D12Resource>& resource, Uint& shaderResourceViewIndex, Uint* unorderedAccessViewIndices);

		void CreateCaptureResources(ID3D12Device* device);

		/// [EN] Fills the environment cube (all 6 faces) from the current HDR
		///      equirect source, then convolves it into irradiance / prefilter.
		/// [JP] 現在の HDR equirect ソースから environment キューブ（6 面）を充填し、
		///      irradiance / prefilter へ畳み込む。
		void GenerateStaticEnvironment(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap);

		/// [EN] Convolves a source cube (SRV in a shader-resource state) into the
		///      irradiance / prefilter cubes.
		/// [JP] ソースキューブ（シェーダーリソース状態）を irradiance / prefilter
		///      キューブへ畳み込む。
		void ConvolveFromSource(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, Uint sourceShaderResourceViewIndex);

		void UpdateFaceCamera(Uint face, Uint lightIndex);

		/// [EN] structuredAddress != 0 additionally binds root parameter 3
		///      (structured indices) — required by ProceduralSkyToCubeCS.hlsl.
		/// [JP] structuredAddress != 0 の場合はルートパラメータ3(structured
		///      indices)も追加でバインドする — ProceduralSkyToCubeCS.hlsl が要求。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, ID3D12PipelineState* pipeline, const SkyGenerateConstant& data, Uint groupsX, Uint groupsY, Uint groupsZ, D3D12_GPU_VIRTUAL_ADDRESS structuredAddress = 0);

		void GenerateProceduralEnvironment(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS structuredAddress);

		void Transition(D3D12CommandList* cmdList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, Uint subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		void UnorderedAccessBarrier(D3D12CommandList* cmdList, ID3D12Resource* resource);

	private:
		static constexpr Uint invalidIndex_ = 0xFFFFFFFF;

		static constexpr Uint environmentSize_ = 512;
		static constexpr Uint environmentMipLevels_ = 10;
		static constexpr Uint irradianceSize_ = 32;
		static constexpr Uint prefilterSize_ = 128;
		static constexpr Uint prefilterMipLevels_ = 5;

		static constexpr DXGI_FORMAT cubeFormat_ = DXGI_FORMAT_R16G16B16A16_FLOAT;

		static constexpr Uint brdfLookupTableSize_ = 512;
		static constexpr Uint brdfSampleCount_ = 1024;
		static constexpr Uint prefilterSampleCount_ = 256;
		static constexpr Uint maxGenerateDispatches_ = 16;
		static constexpr DXGI_FORMAT brdfLookupTableFormat_ = DXGI_FORMAT_R16G16_FLOAT;

		static constexpr DXGI_FORMAT captureDepthFormat_ = DXGI_FORMAT_D32_FLOAT;
		static constexpr Float captureNearPlane_ = 0.1f;
		static constexpr Float captureFarPlane_ = 1000.0f;

		ID3D12Device* device_ = nullptr;
		BindlessHeap* bindlessHeap_ = nullptr;

		RootSignature* rootSignature_ = nullptr;
		Handle<RootSignature> rootSignatureHandle_;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> equirectToCubePipeline_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> diffuseIrradiancePipeline_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> specularPrefilterPipeline_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> brdfLookupTablePipeline_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> proceduralSkyToCubePipeline_;

		DynamicArray<ResourcePtr<ConstantBuffer<SkyGenerateConstant>>> constantBuffers_;
		Uint dispatchCursor_ = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> brdfLookupTableResource_;
		Uint brdfLookupTableShaderResourceViewIndex_ = invalidIndex_;
		Uint brdfLookupTableUnorderedAccessViewIndex_ = invalidIndex_;
		Bool brdfLookupTableGenerated_ = false;

		/// [EN] Sky-only cube: skybox background + static IBL / Realtime sky base.
		/// [JP] 空だけのキューブ: スカイボックス背景 + 静的IBL / Realtime空ベース。
		Microsoft::WRL::ComPtr<ID3D12Resource> environmentResource_;
		Uint environmentShaderResourceViewIndex_ = invalidIndex_;
		Uint environmentUnorderedAccessViewIndices_[environmentMipLevels_] = {};

		/// [EN] Sky + captured scene cube (Realtime): convolution source for IBL.
		/// [JP] 空 + キャプチャシーンのキューブ（Realtime）: IBL 畳み込みソース。
		Microsoft::WRL::ComPtr<ID3D12Resource> dynamicResource_;
		Uint dynamicShaderResourceViewIndex_ = invalidIndex_;
		Uint dynamicUnorderedAccessViewIndex_ = invalidIndex_;
		DescriptorHeap dynamicRenderTargetViewHeap_;

		Microsoft::WRL::ComPtr<ID3D12Resource> irradianceResource_;
		Uint irradianceShaderResourceViewIndex_ = invalidIndex_;
		Uint irradianceUnorderedAccessViewIndex_ = invalidIndex_;

		Microsoft::WRL::ComPtr<ID3D12Resource> prefilterResource_;
		Uint prefilterShaderResourceViewIndex_ = invalidIndex_;
		Uint prefilterUnorderedAccessViewIndices_[prefilterMipLevels_] = {};

		Microsoft::WRL::ComPtr<ID3D12Resource> captureDepthResource_;
		DescriptorHeap captureDepthStencilViewHeap_;
		ResourcePtr<ConstantBuffer<SceneConstantBuffer>> captureSceneConstantBuffer_;
		ResourcePtr<ConstantBuffer<ConstantIndices>> captureConstantIndicesBuffer_;

		/// [EN] Current sky source state (resolved each Gather).
		/// [JP] 現在の空ソース状態（Gather 毎に解決）。
		Bool hasSkymap_ = false;
		Uint sourceEquirectShaderResourceViewIndex_ = invalidIndex_;
		Uint generatedSourceShaderResourceViewIndex_ = invalidIndex_;
		Float intensity_ = 1.0f;
		SkyLight::CaptureType captureType_ = SkyLight::CaptureType::Lietime;
		Float captureDuration_ = 1.0f;
		Float captureTimer_ = 0.0f;
		Uint currentCaptureFace_ = 0;

		/// [EN] Procedural-sky IBL state (SetProceduralSky). Regenerates when
		///      the settings hash changes or periodically (sun rotation isn't
		///      in the hash). Generated/skymap states invalidate each other so
		///      toggling between modes always regenerates correctly.
		/// [JP] プロシージャル空 IBL の状態(SetProceduralSky)。設定ハッシュの
		///      変化時、または定期的に再生成する(太陽の回転はハッシュに含まれ
		///      ないため)。生成状態はスカイマップ側と相互に無効化し合うので、
		///      モードを切り替えても必ず正しく再生成される。
		Bool proceduralSkyEnabled_ = false;
		Uint32 proceduralSkyHash_ = 0;
		Uint proceduralSkyLightIndex_ = 0;
		Float proceduralSkyTime_ = 0.0f;
		Uint32 generatedProceduralSkyHash_ = 0;
		Bool proceduralSkyGenerated_ = false;
		Uint proceduralSkyRefreshCounter_ = 0;
		static constexpr Uint proceduralSkyRefreshInterval_ = 120;
	};
}
