#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>

namespace SeedCore
{
	class ShaderCache;

	class VertexShader;
	class AmplificationShader;
	class MeshShader;
	class PixelShader;
	class ComputeShader;

	/**
	* [EN]
	* Manages PSO creation for model rendering.
	*
	* Opaque PSOs (G-Buffer output, 4 render targets):
	*   - Static: ModelAS + StaticModelMS + StaticModelPS
	*   - Skeletal: ModelAS + SkeletalModelMS + SkeletalModelPS
	*
	* Transparent PSOs (UAV-only output for PPLL OIT):
	*   - Static: ModelAS + StaticModelMS + ModelTransparentPS
	*   - Skeletal: ModelAS + SkeletalModelMS + ModelTransparentPS
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
	*   - 静的: ModelAS + StaticModelMS + ModelTransparentPS
	*   - スケルタル: ModelAS + SkeletalModelMS + ModelTransparentPS
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

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateModelCulling()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateGeometryBufferCulling()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateModelTransparentCulling()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateModelSilhouetteCulling()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateFurShellCulling()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateDepthPrepass()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateDepthPrepassDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateStaticDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSkeletal()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSkeletalDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateStaticTransparent()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateStaticTransparentDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSkeletalTransparent()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSkeletalTransparentDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateFurShell()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateFurShellDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateResolve()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateComposite()const;

		/// [EN] Unlit preview PSOs for the Timeline panel's 3D model preview:
		///      ModelAS + Static/Skeletal MS + ModelPreviewPS (base color +
		///      emissive only, no lighting). Single R16G16B16A16_FLOAT RT,
		///      depth write on, reverse-Z. Deliberately its own PS so the
		///      preview never reads constant_indices.light_index_ or any
		///      shared lighting state.
		/// [JP] Timelineパネルの3Dモデルプレビュー用アンリットPSO: ModelAS +
		///      Static/Skeletal MS + ModelPreviewPS（baseColor + emissiveの
		///      み、ライティング無し）。単一R16G16B16A16_FLOAT RT、深度書き込み
		///      あり、reverse-Z。あえて専用PSにすることでプレビューが
		///      constant_indices.light_index_ や共有ライティング状態を
		///      一切読まないようにする。
		[[nodiscard]] ID3D12PipelineState* GetPipelineStatePreviewStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStatePreviewStaticDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStatePreviewSkeletal()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStatePreviewSkeletalDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateAvatarPreview()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateAvatarPreviewDoubleSided()const;

		/// [EN] Wireframe debug PSOs: Static/Skeletal MS + WireframePS, wireframe
		///      rasterizer, single R16G16B16A16_FLOAT RT, depth read (no write) so
		///      the wires are occluded by the scene depth. Editor view-mode only.
		/// [JP] ワイヤーフレーム デバッグ PSO: Static/Skeletal MS ＋ WireframePS、
		///      ワイヤーフレームラスタライザ、単一 R16G16B16A16_FLOAT RT、深度は
		///      読み取りのみ（書き込みなし）でシーン深度に遮蔽される。エディタの
		///      表示モード専用。
		[[nodiscard]] ID3D12PipelineState* GetPipelineStateWireframeStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateWireframeStaticDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateWireframeSkeletal()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateWireframeSkeletalDoubleSided()const;

		/// [EN] Meshlet visualization PSOs: flat color per meshlet (solid fill).
		/// [JP] メッシュレット可視化 PSO: メッシュレットごとに単色（ソリッド塗り）。
		[[nodiscard]] ID3D12PipelineState* GetPipelineStateMeshletStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateMeshletStaticDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateMeshletSkeletal()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateMeshletSkeletalDoubleSided()const;

		/// [EN] Silhouette PSOs: Static/Skeletal MS + ModelSilhouettePS,
		///      single R8_UNORM RT, depth off. The fullscreen edge-detect composite
		///      that reads this mask is generic (not model-specific) and lives on
		///      Renderer instead, shared by every actor type that can be selected.
		/// [JP] シルエット PSO: Static/Skeletal MS + ModelSilhouettePS、
		///      単一 R8_UNORM RT、深度オフ。このマスクを読むフルスクリーンのエッジ
		///      検出合成は Model 固有ではないため Renderer 側が持ち、選択され得る
		///      全アクター種別で共有する。
		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSilhouetteStatic()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSilhouetteStaticDoubleSided()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSilhouetteSkeletal()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSilhouetteSkeletalDoubleSided()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<AmplificationShader> amplificationShader_;
		Handle<AmplificationShader> geometryBufferAmplificationShader_;
		Handle<AmplificationShader> transparentAmplificationShader_;
		Handle<AmplificationShader> furShellAmplificationShader_;

		Handle<ComputeShader> modelCullingComputeShader_;
		Handle<ComputeShader> geometryBufferCullingComputeShader_;
		Handle<ComputeShader> modelTransparentCullingComputeShader_;
		Handle<ComputeShader> modelSilhouetteCullingComputeShader_;
		Handle<ComputeShader> furShellCullingComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectModelCulling_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectGeometryBufferCulling_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectModelTransparentCulling_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectModelSilhouetteCulling_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectFurShellCulling_;

		Handle<VertexShader> depthPrepassVertexShader_;
		Handle<MeshShader> depthPrepassMeshShader_;
		Handle<PixelShader> depthPrepassPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectDepthPrepass_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectDepthPrepassDoubleSided_;

		Handle<VertexShader> staticVertexShader_;
		Handle<MeshShader> staticMeshShader_;
		Handle<PixelShader> staticPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectStaticDoubleSided_;

		Handle<VertexShader> skeletalVertexShader_;
		Handle<MeshShader> skeletalMeshShader_;
		Handle<PixelShader> skeletalPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSkeletal_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSkeletalDoubleSided_;

		Handle<PixelShader> transparentPixelShader_;

		Handle<VertexShader> furShellVertexShader_;
		Handle<MeshShader> furShellMeshShader_;
		Handle<PixelShader> furShellPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectStaticTransparent_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectStaticTransparentDoubleSided_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSkeletalTransparent_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSkeletalTransparentDoubleSided_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectFurShell_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectFurShellDoubleSided_;

		Handle<VertexShader> resolveVertexShader_;
		Handle<MeshShader> resolveMeshShader_;
		Handle<PixelShader> resolvePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectResolve_;

		Handle<VertexShader> compositeVertexShader_;
		Handle<MeshShader> compositeMeshShader_;
		Handle<PixelShader> compositePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectComposite_;


		Handle<PixelShader> previewPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectPreviewStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectPreviewStaticDoubleSided_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectPreviewSkeletal_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectPreviewSkeletalDoubleSided_;

		Handle<PixelShader> avatarPreviewPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectAvatarPreview_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectAvatarPreviewDoubleSided_;

		Handle<PixelShader> wireframePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectWireframeStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectWireframeStaticDoubleSided_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectWireframeSkeletal_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectWireframeSkeletalDoubleSided_;

		Handle<PixelShader> meshletPixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectMeshletStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectMeshletStaticDoubleSided_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectMeshletSkeletal_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectMeshletSkeletalDoubleSided_;

		Handle<AmplificationShader> silhouetteAmplificationShader_;
		Handle<PixelShader> silhouettePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSilhouetteStatic_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSilhouetteStaticDoubleSided_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSilhouetteSkeletal_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSilhouetteSkeletalDoubleSided_;

		Handle<RootSignature> modelRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
