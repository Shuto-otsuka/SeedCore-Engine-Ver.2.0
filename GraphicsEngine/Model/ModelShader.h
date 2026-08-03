#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>

namespace SeedCore
{
	class ShaderCache;

	class AmplificationShader;
	class MeshShader;
	class PixelShader;

	/**
	* [EN]
	* Manages PSO creation for model rendering.
	*
	* Opaque PSOs (G-Buffer output, 4 render targets):
	*   - Static: ModelAS + StaticModelMS + StaticModelPS
	*   - Skeletal: ModelAS + SkeletalModelMS + SkeletalModelPS
	*
	* Transparent PSOs (UAV-only output for PPLL OIT):
	*   - Static: ModelAS + StaticModelMS + TransparentModelPS
	*   - Skeletal: ModelAS + SkeletalModelMS + TransparentModelPS
	*
	* Resolve PSO (fullscreen, alpha blend over opaque):
	*   - OITResolveMS + OITResolvePS
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* モデルレンダリング用の PSO 管理。
	*
	* 不透明 PSO (G-Buffer 出力、4 レンダーターゲット):
	*   - 静的: ModelAS + StaticModelMS + StaticModelPS
	*   - スケルタル: ModelAS + SkeletalModelMS + SkeletalModelPS
	*
	* 透明 PSO (PPLL OIT 用 UAV のみ出力):
	*   - 静的: ModelAS + StaticModelMS + TransparentModelPS
	*   - スケルタル: ModelAS + SkeletalModelMS + TransparentModelPS
	*
	* リゾルブ PSO (フルスクリーン、不透明上にアルファブレンド):
	*   - OITResolveMS + OITResolvePS
	*/
	class ModelShader
	{
	public:
		ModelShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ModelShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateDepthPrepass()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSkeletal()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateStaticTransparent()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSkeletalTransparent()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateResolve()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateComposite()const;

		/// [EN] Forward-lit PSOs that render opaque models into an environment
		///      cube face (single R16G16B16A16_FLOAT RT) for the Realtime skymap
		///      capture. Reuse ModelAS (no Hi-Z, capture view) + Static/Skeletal
		///      MS + SkyForwardPS.
		/// [JP] Realtime スカイマップキャプチャ用に不透明モデルを environment
		///      キューブ面（単一 R16G16B16A16_FLOAT RT）へフォワード描画する PSO。
		///      ModelAS（Hi-Z なし・キャプチャ視点）＋ Static/Skeletal MS ＋
		///      SkyForwardPS を再利用。
		[[nodiscard]] ID3D12PipelineState* GetPipelineStateForwardCaptureStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateForwardCaptureSkeletal()const;

		/// [EN] Unlit preview PSOs for the Timeline panel's 3D model preview:
		///      ModelAS + Static/Skeletal MS + ModelPreviewPS (base color +
		///      emissive only, no lighting). Same target shape as
		///      ForwardCapture (single R16G16B16A16_FLOAT RT, depth write on,
		///      reverse-Z) but deliberately its own PS so the preview never
		///      reads constant_indices.light_index_ or any shared lighting
		///      state.
		/// [JP] Timelineパネルの3Dモデルプレビュー用アンリットPSO: ModelAS +
		///      Static/Skeletal MS + ModelPreviewPS（baseColor + emissiveの
		///      み、ライティング無し）。ターゲット形状はForwardCaptureと同じ
		///      （単一R16G16B16A16_FLOAT RT、深度書き込みあり、reverse-Z）だが、
		///      あえて専用PSにすることでプレビューがconstant_indices.light_index_
		///      や共有ライティング状態を一切読まないようにする。
		[[nodiscard]] ID3D12PipelineState* GetPipelineStatePreviewStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStatePreviewSkeletal()const;

		/// [EN] Wireframe debug PSOs: Static/Skeletal MS + WireframePS, wireframe
		///      rasterizer, single R16G16B16A16_FLOAT RT, depth read (no write) so
		///      the wires are occluded by the scene depth. Editor view-mode only.
		/// [JP] ワイヤーフレーム デバッグ PSO: Static/Skeletal MS ＋ WireframePS、
		///      ワイヤーフレームラスタライザ、単一 R16G16B16A16_FLOAT RT、深度は
		///      読み取りのみ（書き込みなし）でシーン深度に遮蔽される。エディタの
		///      表示モード専用。
		[[nodiscard]] ID3D12PipelineState* GetPipelineStateWireframeStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateWireframeSkeletal()const;

		/// [EN] Meshlet visualization PSOs: flat color per meshlet (solid fill).
		/// [JP] メッシュレット可視化 PSO: メッシュレットごとに単色（ソリッド塗り）。
		[[nodiscard]] ID3D12PipelineState* GetPipelineStateMeshletStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateMeshletSkeletal()const;

		/// [EN] Selection outline mask PSOs: Static/Skeletal MS + SelectionMaskPS,
		///      single R8_UNORM RT, depth off. The fullscreen edge-detect composite
		///      that reads this mask is generic (not model-specific) and lives on
		///      Renderer instead, shared by every actor type that can be selected.
		/// [JP] 選択アウトラインマスク PSO: Static/Skeletal MS + SelectionMaskPS、
		///      単一 R8_UNORM RT、深度オフ。このマスクを読むフルスクリーンのエッジ
		///      検出合成は Model 固有ではないため Renderer 側が持ち、選択され得る
		///      全アクター種別で共有する。
		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSelectionMaskStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSelectionMaskSkeletal()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<AmplificationShader> amplificationShader_;
		Handle<AmplificationShader> geometryBufferAmplificationShader_;
		Handle<AmplificationShader> transparentAmplificationShader_;

		Handle<MeshShader> depthPrepassMeshShader_;
		Handle<PixelShader> depthPrepassPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectDepthPrepass_;

		Handle<MeshShader> staticMeshShader_;
		Handle<PixelShader> staticPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectStatic_;

		Handle<MeshShader> skeletalMeshShader_;
		Handle<PixelShader> skeletalPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSkeletal_;

		Handle<PixelShader> transparentPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectStaticTransparent_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSkeletalTransparent_;

		Handle<MeshShader> resolveMeshShader_;
		Handle<PixelShader> resolvePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectResolve_;

		Handle<MeshShader> compositeMeshShader_;
		Handle<PixelShader> compositePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectComposite_;

		Handle<PixelShader> forwardCapturePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectForwardCaptureStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectForwardCaptureSkeletal_;

		Handle<PixelShader> previewPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectPreviewStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectPreviewSkeletal_;

		Handle<PixelShader> wireframePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectWireframeStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectWireframeSkeletal_;

		Handle<PixelShader> meshletPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectMeshletStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectMeshletSkeletal_;

		Handle<AmplificationShader> selectionAmplificationShader_;
		Handle<PixelShader> selectionMaskPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSelectionMaskStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSelectionMaskSkeletal_;

		Handle<RootSignature> modelRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
