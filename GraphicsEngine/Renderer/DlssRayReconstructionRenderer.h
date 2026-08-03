#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/DLSS/DlssManager.h>
#include <GraphicsEngine/DLSS/DlssNormalRoughnessShader.h>
#include <GraphicsEngine/DLSS/DlssBackgroundVelocityShader.h>
#include <GraphicsEngine/Raytracing/RaytracingView.h>
#include <GraphicsEngine/System/SceneSystem.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/**
	* [EN]
	* Runs NVIDIA DLSS Ray Reconstruction (DLSS-RR) on one view's raw noisy
	* composited color (Model/DeferredCompositePS.hlsl's output, before any
	* per-effect denoising or tone mapping) and produces a denoised,
	* 1280x720->3840x2160 upscaled, linear HDR result for
	* PostProcessRenderer's tone-map pass to consume. Two independent
	* per-view Streamline viewport chains (Editor=0, Game=1) so each keeps
	* its own DLSS motion/history state.
	*
	* Before Tag()/Evaluate() this also synthesizes the RGB=normal/A=roughness
	* buffer DLSS-RR expects (DlssNormalRoughnessCS.hlsl decodes the
	* G-Buffer's packed RT1) since the engine's own G-Buffer has no such
	* channel natively.
	*
	* This is the alternative to Shadow/AO/GI's own per-effect spatio-temporal
	* denoisers - when active, those renderers bypass their own denoise
	* dispatch (see e.g. GlobalIlluminationRenderer::Dispatch's
	* useDlssRayReconstruction branch) since double-denoising would fight
	* DLSS-RR's own denoiser.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* NVIDIA DLSS Ray Reconstruction(DLSS-RR)を、あるビューの生のノイズ入り
	* 合成カラー(Model/DeferredCompositePS.hlsl の出力、エフェクトごとの
	* デノイズやトーンマップより前)に対して実行し、デノイズ済み・
	* 1280x720→3840x2160 アップスケール済みのリニア HDR 結果を
	* PostProcessRenderer のトーンマップパスへ渡す。ビューごとに独立した
	* Streamline ビューポートチェーン(Editor=0, Game=1)を持つので、
	* それぞれ独立した DLSS モーション/履歴状態を保つ。
	*
	* Tag()/Evaluate() の前に、DLSS-RR が要求する RGB=法線/A=ラフネスバッファも
	* 合成する(DlssNormalRoughnessCS.hlsl が G-Buffer のパック済み RT1を
	* デコード) — エンジン自体の G-Buffer にはこの形のチャンネルが無いため。
	*
	* Shadow/AO/GI 自前の空間+時間デノイザの代替経路 — 有効な間、それらの
	* Renderer は自前デノイズのディスパッチをバイパスする(例:
	* GlobalIlluminationRenderer::Dispatch の useDlssRayReconstruction 分岐参照)。
	* 二重デノイズは DLSS-RR 自身のデノイザと衝突するため。
	*/
	class DlssRayReconstructionRenderer
	{
	public:
		DlssRayReconstructionRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~DlssRayReconstructionRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight);

		/// [EN] The full per-view pass: normal+roughness/diffuse-albedo/
		///      specular-albedo synthesis, DlssManager Tag(), then
		///      EvaluateRayReconstruction() into this view's 3840x2160 output.
		///      colorResource must be Model/DeferredCompositePS.hlsl's raw
		///      composited HDR result (PIXEL_SHADER_RESOURCE on entry);
		///      depth/velocity come from the same frame's G-Buffer
		///      (GeometryBuffer::DepthResource()/ColorResource(3)); albedo/
		///      specular-albedo/normal-roughness are synthesized internally
		///      from G-Buffer RT0/RT1/RT2 (see View::diffuseAlbedoResource_/
		///      specularAlbedoResource_/normalRoughnessResource_). Leaves the
		///      3840x2160 output in PIXEL_SHADER_RESOURCE state for
		///      PostProcessRenderer to sample as its tone-map source.
		///      dlssManager must already have had BeginFrame() called once
		///      this Present-frame (see Graphics::Begin()).
		/// [JP] 1ビューぶんの全処理: 法線+ラフネス/拡散アルベド/鏡面アルベド合成、
		///      DlssManager の Tag()、続けて EvaluateRayReconstruction() を
		///      このビューの3840x2160出力へ。colorResource は
		///      Model/DeferredCompositePS.hlsl の生の合成HDR結果であること
		///      (呼び出し時点でPIXEL_SHADER_RESOURCE)。depth/velocity は
		///      同フレームのG-Buffer(GeometryBuffer::DepthResource()/
		///      ColorResource(3))から。albedo/鏡面アルベド/法線ラフネスは
		///      G-Buffer RT0/RT1/RT2 から内部で合成する(View::
		///      diffuseAlbedoResource_/specularAlbedoResource_/
		///      normalRoughnessResource_ 参照)。3840x2160出力は
		///      PostProcessRenderer がトーンマップソースとしてサンプルできるよう
		///      PIXEL_SHADER_RESOURCE状態で終える。dlssManager はこの
		///      Presentフレーム内で既に BeginFrame() 済みであること
		///      (Graphics::Begin() 参照)。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view, DlssManager* dlssManager, const SceneConstantBuffer& scene, ID3D12Resource* colorResource, ID3D12Resource* depthResource, ID3D12Resource* velocityResource, Uint32 sourceWidth, Uint32 sourceHeight, DlssMode mode);

		/// [EN] This view's 3840x2160 denoised+upscaled linear HDR output -
		///      what Renderer passes as PostProcessRenderer::Dispatch's
		///      sourceColorResource when DLSS-RR is active.
		/// [JP] このビューの3840x2160デノイズ+アップスケール済みリニアHDR出力 —
		///      DLSS-RR有効時に Renderer が PostProcessRenderer::Dispatch の
		///      sourceColorResource として渡す対象。
		[[nodiscard]] ID3D12Resource* OutputResource(RaytracingView view)const;

		/// [EN] Bindless SRV index of the same resource, for
		///      PostProcessRenderer::PrepareView's sourceColorIndex.
		/// [JP] 同リソースの bindless SRV インデックス。
		///      PostProcessRenderer::PrepareView の sourceColorIndex に渡す。
		[[nodiscard]] Uint32 OutputShaderResourceViewIndex(RaytracingView view)const;

	private:
		struct View
		{
			/// [EN] RGB=normal/A=roughness, native (1280x720) resolution.
			/// [JP] RGB=法線/A=ラフネス、ネイティブ(1280x720)解像度。
			Microsoft::WRL::ComPtr<ID3D12Resource> normalRoughnessResource_;
			D3D12_RESOURCE_STATES normalRoughnessState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 normalRoughnessUnorderedAccessViewIndex_ = 0;

			Microsoft::WRL::ComPtr<ID3D12Resource> specularAlbedoResource_;
			D3D12_RESOURCE_STATES specularAlbedoState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 specularAlbedoUnorderedAccessViewIndex_ = 0;

			Microsoft::WRL::ComPtr<ID3D12Resource> diffuseAlbedoResource_;
			D3D12_RESOURCE_STATES diffuseAlbedoState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 diffuseAlbedoUnorderedAccessViewIndex_ = 0;

			/// [EN] Denoised+upscaled (3840x2160) linear HDR output.
			/// [JP] デノイズ+アップスケール(3840x2160)済みリニアHDR出力。
			Microsoft::WRL::ComPtr<ID3D12Resource> outputResource_;
			D3D12_RESOURCE_STATES outputState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 outputUnorderedAccessViewIndex_ = 0;
			Uint32 outputShaderResourceViewIndex_ = 0;
		};

		DlssNormalRoughnessShader normalRoughnessShader_;
		DlssBackgroundVelocityShader backgroundVelocityShader_;

		View editorView_;
		View gameView_;

		[[nodiscard]] View& ViewFor(RaytracingView view);

		void CreateViewResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight);

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Uint32 width_ = 0;
		Uint32 height_ = 0;

		Uint32 outputWidth_ = 0;
		Uint32 outputHeight_ = 0;

		/// [EN] Logs the PSO-creation-failed warning once instead of every frame.
		/// [JP] PSO 作成失敗の警告を毎フレームでなく 1 度だけログ出力する。
		Bool pipelineStateMissingLogged_ = false;
	};
}
