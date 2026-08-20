#include <GraphicsEngine/Model/ModelShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/AmplificationShader.h>
#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>

namespace SeedCore
{
	ModelShader::ModelShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void ModelShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		modelRootSignature_ = rootSignature_.GetOrCreate(device);

		amplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Model/Opaque/ModelAS.hlsl"));
		/// [EN] G-Buffer passes use a Hi-Z occlusion-culling variant of the AS.
		///      The prepass keeps the plain AS because it generates the Hi-Z data.
		/// [JP] G-Buffer パスは Hi-Z オクルージョンカリング付きの AS を使う。
		///      プリパスは Hi-Z データを生成する側なので通常の AS のまま。
		geometryBufferAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Model/Opaque/ModelGeometryBufferAS.hlsl"));
		transparentAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Model/Transparent/ModelTransparentAS.hlsl"));

		/// [EN] Depth prepass PSO: ModelAS + DepthPrepassMS + DepthPrepassPS (alpha-cutout
		///      clip only), depth-only output. The PS is required so masked (alphaMode=MASK)
		///      holes do not write depth and occlude geometry behind them.
		/// [JP] デプスプリパス PSO: ModelAS + DepthPrepassMS + DepthPrepassPS（カットアウトの
		///      clip のみ）、デプスのみ出力。カットアウト（alphaMode=MASK）の穴が深度を書いて
		///      後ろのジオメトリを隠さないよう、PS が必須。
		{
			depthPrepassMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Model/Opaque/DepthPrepassMS.hlsl"));
			depthPrepassPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/Opaque/DepthPrepassPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(amplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(depthPrepassMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(depthPrepassPixelShader_)->Bytecode();
			/// [EN] Cull off: glTF materials are frequently doubleSided (thin wings, hair, cloth).
			///      Per-material cull selection requires splitting draw batches — until then,
			///      render models double-sided.
			/// [JP] カリング無効: glTF マテリアルは doubleSided（薄い翼・髪・布）が多い。
			///      マテリアル別のカリング切替はドローバッチ分割が必要なため、
			///      それまでモデルは両面描画にする。
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOnReverseZ);
			psokey.renderTargetViewCount_ = 0;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectDepthPrepass_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Static model PSO: ModelAS + StaticModelMS + StaticModelPS → G-Buffer
		///      visibility id only (RT4, R32G32B32A32_UINT). RT0-3 are rewritten
		///      afterward by Model/Material/MaterialResolveCS.hlsl - see GeometryBuffer::BeginVisibility.
		/// [JP] 静的モデル PSO: ModelAS + StaticModelMS + StaticModelPS →
		///      G-Buffer visibility id のみ(RT4, R32G32B32A32_UINT)。RT0-3 はこの後
		///      Model/Material/MaterialResolveCS.hlsl が書き直す - GeometryBuffer::BeginVisibility 参照。
		{
			staticMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Model/Opaque/StaticModelMS.hlsl"));
			staticPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/Opaque/StaticModelPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(geometryBufferAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(staticMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(staticPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOffReverseZ);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R32G32B32A32_UINT;			// visibility id (instance_index, pack(meshlet_index, triangle_in_meshlet_index)) + asuint(texcoord) - see GeometryBuffer::BeginVisibility
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectStatic_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Skeletal model PSO: ModelAS + SkeletalModelMS + SkeletalModelPS →
		///      G-Buffer visibility id only (RT4, R32G32B32A32_UINT). Same as the static PSO above.
		/// [JP] スケルタルモデル PSO: ModelAS + SkeletalModelMS + SkeletalModelPS →
		///      G-Buffer visibility id のみ(RT4, R32G32B32A32_UINT)。上の静的PSOと同様。
		{
			skeletalMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Model/Opaque/SkeletalModelMS.hlsl"));
			skeletalPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/Opaque/SkeletalModelPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(geometryBufferAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(skeletalMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(skeletalPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOffReverseZ);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R32G32B32A32_UINT;			// visibility id (instance_index, pack(meshlet_index, triangle_in_meshlet_index)) + asuint(texcoord) - see GeometryBuffer::BeginVisibility
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectSkeletal_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Preview PSOs: ModelAS + Static/Skeletal MS, single
		///      R16G16B16A16_FLOAT RT, depth write on, reverse-Z, with the
		///      unlit ModelPreviewPS.
		/// [JP] プレビューPSO: ModelAS + Static/Skeletal MS、単一
		///      R16G16B16A16_FLOAT RT、深度書き込みあり、reverse-Z、
		///      アンリットのModelPreviewPSを使う。
		{
			previewPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/ModelPreviewPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(amplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(staticMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(previewPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOnReverseZ);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectPreviewStatic_ = pipelineStateObject_.GetOrCreate(device, psokey);

			psokey.meshShader_ = shaderCache.GetMeshShader(skeletalMeshShader_)->Bytecode();
			pipelineStateObjectPreviewSkeletal_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Wireframe debug PSOs: Static/Skeletal MS + WireframePS, wireframe
		///      rasterizer, single R16G16B16A16_FLOAT RT (the editor frame buffer),
		///      depth read-only reverse-Z so wires are occluded by the scene depth.
		/// [JP] ワイヤーフレーム デバッグ PSO: Static/Skeletal MS ＋ WireframePS、
		///      ワイヤーフレームラスタライザ、単一 R16G16B16A16_FLOAT RT（エディタ
		///      フレームバッファ）、深度は読み取りのみ reverse-Z でシーン深度に遮蔽。
		{
			wireframePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/WireframePS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(geometryBufferAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(staticMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(wireframePixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::WireNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOffReverseZ);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectWireframeStatic_ = pipelineStateObject_.GetOrCreate(device, psokey);

			psokey.meshShader_ = shaderCache.GetMeshShader(skeletalMeshShader_)->Bytecode();
			pipelineStateObjectWireframeSkeletal_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Meshlet visualization PSOs: Static/Skeletal MS + MeshletPS, solid
		///      fill, single R16G16B16A16_FLOAT RT, depth read-only reverse-Z. Each
		///      meshlet is flat-colored by a hash of its index. Editor mode only.
		/// [JP] メッシュレット可視化 PSO: Static/Skeletal MS ＋ MeshletPS、ソリッド
		///      塗り、単一 R16G16B16A16_FLOAT RT、深度は読み取りのみ reverse-Z。
		///      メッシュレットごとに index のハッシュで単色塗り。エディタ表示モード専用。
		{
			meshletPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/MeshletPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(geometryBufferAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(staticMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(meshletPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOffReverseZ);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectMeshletStatic_ = pipelineStateObject_.GetOrCreate(device, psokey);

			psokey.meshShader_ = shaderCache.GetMeshShader(skeletalMeshShader_)->Bytecode();
			pipelineStateObjectMeshletSkeletal_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Selection outline mask PSOs: Static/Skeletal MS + SelectionMaskPS,
		///      solid fill, single R8_UNORM RT, depth off. The mask draws the
		///      selected mesh's full silhouette regardless of nearer, unselected
		///      occluders — depth-testing it against the scene depth would carve
		///      occluder-shaped holes into the mask, and the edge-detect composite
		///      would then trace an outline around the occluder instead of just
		///      leaving that part unoutlined. A dedicated AS (ModelSelectionAS)
		///      filters to selected_ != 0 instances only.
		/// [JP] 選択アウトラインマスク PSO: Static/Skeletal MS + SelectionMaskPS、
		///      ソリッド塗り、単一 R8_UNORM RT、深度オフ。シーン深度でテストすると
		///      手前の未選択オブジェクトの形にマスクへ穴が開き、エッジ検出合成が
		///      その穴の境界（＝手前のオブジェクトの輪郭）までアウトラインとして
		///      拾ってしまうため、選択メッシュのシルエットは遮蔽を無視して全体を
		///      塗る。専用 AS（ModelSelectionAS）が selected_ != 0 のインスタンス
		///      だけを通す。
		{
			selectionAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Model/ModelSelectionAS.hlsl"));
			selectionMaskPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/SelectionMaskPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(selectionAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(staticMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(selectionMaskPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R8_UNORM;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_UNKNOWN;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectSelectionMaskStatic_ = pipelineStateObject_.GetOrCreate(device, psokey);

			psokey.meshShader_ = shaderCache.GetMeshShader(skeletalMeshShader_)->Bytecode();
			pipelineStateObjectSelectionMaskSkeletal_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Transparent PSOs: UAV-only output (no RTs), depth read without write.
		/// [JP] 透明 PSO: UAV のみ出力（RT なし）、深度読み取りのみ（書き込みなし）。
		{
			transparentPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/Transparent/ModelTransparentPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(transparentAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(staticMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(transparentPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOffReverseZ);
			psokey.renderTargetViewCount_ = 0;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectStaticTransparent_ = pipelineStateObject_.GetOrCreate(device, psokey);

			psokey.meshShader_ = shaderCache.GetMeshShader(skeletalMeshShader_)->Bytecode();
			pipelineStateObjectSkeletalTransparent_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] OIT Resolve PSO: fullscreen, alpha blend onto opaque scene.
		/// [JP] OIT リゾルブ PSO: フルスクリーン、不透明シーン上にアルファブレンド。
		{
			resolveMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Model/Transparent/OITResolveMS.hlsl"));
			resolvePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/Transparent/OITResolvePS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.meshShader_ = shaderCache.GetMeshShader(resolveMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(resolvePixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			/// [JP] OITResolvePS.hlsl は事前乗算済み(premultiplied)の rgb を出力する
			///      ので、SrcBlend が ONE の AlphaPremultiplied を使う。通常の Alpha
			///      (SrcBlend=SRC_ALPHA)だとアルファが二重に掛かって透明面が暗くなる。
			psokey.blendDesc_ = BlendState::Get(BlendStateType::AlphaPremultiplied);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_UNKNOWN;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectResolve_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Deferred lighting PSO: fullscreen, reads G-Buffer SRVs, outputs to FrameBuffer.
		/// [JP] ディファードライティング PSO: フルスクリーン、G-Buffer SRV を読み出しフレームバッファに出力。
		{
			compositeMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Model/Opaque/DeferredLightingMS.hlsl"));
			compositePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Model/Opaque/DeferredLightingPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(modelRootSignature_)->Get();
			psokey.meshShader_ = shaderCache.GetMeshShader(compositeMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(compositePixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_UNKNOWN;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectComposite_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateDepthPrepass()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectDepthPrepass_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateStatic()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectStatic_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateSkeletal()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSkeletal_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateStaticTransparent()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectStaticTransparent_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateSkeletalTransparent()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSkeletalTransparent_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateResolve()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectResolve_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateComposite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectComposite_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStatePreviewStatic()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectPreviewStatic_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStatePreviewSkeletal()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectPreviewSkeletal_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateWireframeStatic()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectWireframeStatic_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateWireframeSkeletal()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectWireframeSkeletal_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateMeshletStatic()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectMeshletStatic_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateMeshletSkeletal()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectMeshletSkeletal_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateSelectionMaskStatic()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSelectionMaskStatic_);
	}

	ID3D12PipelineState* ModelShader::GetPipelineStateSelectionMaskSkeletal()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSelectionMaskSkeletal_);
	}

	ID3D12RootSignature* ModelShader::GetRootSignature()const
	{
		return rootSignature_.Get(modelRootSignature_)->Get();
	}
}
