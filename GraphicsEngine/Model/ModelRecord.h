#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/Assert.h>

namespace SeedCore
{
	/**
	* [EN]
	* previousWorld_ is this instance's own world matrix as of the previous
	* frame - see Model.hlsli's ModelTransform::previous_world_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* previousWorld_ はこのインスタンスの前フレーム時点のワールド行列 —
	* Model.hlsli の ModelTransform::previous_world_ 参照。
	*/
	struct ModelTransform
	{
		Matrix world_;
		Matrix inverseTransposeWorld_;
		Matrix previousWorld_;
	};
	SC_STATIC_ASSERT_SIZE(ModelTransform, 192, "Model/Model.hlsli");

	/**
	* [EN]
	* Bindless SRVs of this instance's geometry buffers (compressed vertices,
	* meshlets, meshlet bounds, meshlet-local vertex indices, packed
	* primitive indices), plus meshletOffset_/meshletCount_ - this
	* instance's range into the shared meshlet buffers.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このインスタンスのジオメトリバッファ(圧縮頂点、meshlet、meshlet境界、
	* meshletローカル頂点インデックス、パック済みプリミティブインデックス)の
	* bindless SRVと、meshletOffset_/meshletCount_(共有meshletバッファへの
	* このインスタンスの範囲)。
	*/
	struct ModelGeometry
	{
		Uint vertexBufferIndex_;
		Uint meshletBufferIndex_;
		Uint meshletBoundBufferIndex_;
		Uint vertexIndicesBufferIndex_;
		Uint primitiveIndicesBufferIndex_;
		Uint meshletOffset_;
		Uint meshletCount_;
		Uint modelGeometryPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ModelGeometry, 32, "Model/Model.hlsli");

	/**
	* [EN]
	* Dequantisation AABB for the compressed vertex buffer (Crister::
	* PositionMin etc. - see Model.hlsli's DecodeModelVertex), plus the
	* cumulative LOD error of this cluster and of the next coarser cluster
	* in the chain (FLT_MAX = always keep).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 圧縮頂点バッファの逆量子化AABB(Crister::PositionMinほか — Model.hlsli の
	* DecodeModelVertex 参照)と、このクラスタの累積LOD誤差、チェーン内の次に
	* 粗いクラスタの誤差(FLT_MAX = 常に描画)。
	*/
	struct ModelStreaming
	{
		Vector3 positionMin_;
		Float texcoordMinU_;

		Vector3 positionExtent_;
		Float texcoordMinV_;

		Vector2 texcoordExtent_;
		Float lodError_;
		Float lodErrorNext_;
	};
	SC_STATIC_ASSERT_SIZE(ModelStreaming, 48, "Model/Model.hlsli");

	/**
	* [EN]
	* shadingModel_ selects the lighting response DeferredLightingPS.hlsl
	* evaluates this instance with (see Crister.h's ShadingModel).
	* furLength_/furDensity_/furShellCount_ are ShadingModel::Fur tuning
	* (shell height in metres, per-strand hash-noise density, shell layers
	* above the base surface).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* shadingModel_ は DeferredLightingPS.hlsl がこのインスタンスをどの
	* ライティング応答で評価するか(Crister.h の ShadingModel 参照)。
	* furLength_/furDensity_/furShellCount_ は ShadingModel::Fur の
	* チューニング値(シェルの高さ(メートル)、毛1本ごとのハッシュノイズ密度、
	* ベース面の上に描画するシェル層数)。
	*/
	struct ModelShading
	{
		Uint shadingModel_;
		Float furLength_;
		Float furDensity_;
		Uint furShellCount_;

		Uint doubleSided_;
		Uint blend_;
		Uint selected_;
		Uint modelShadingPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ModelShading, 32, "Model/Model.hlsli");

	/**
	* [EN]
	* skinVertexBufferIndex_ is the bindless SRV of the CompressedModelSkin
	* buffer (0xFFFFFFFF for a static, unskinned instance).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* skinVertexBufferIndex_ は CompressedModelSkin バッファの bindless SRV
	* (静的な非スキンインスタンスでは 0xFFFFFFFF)。
	*/
	struct ModelSkining
	{
		Uint skinIndex_;
		Uint boneOffset_;
		Uint skinVertexBufferIndex_;
		Uint modelSkiningPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ModelSkining, 16, "Model/Model.hlsli");

	/**
	* [EN]
	* Raster morph blend (see the model mesh shaders and
	* Model/Material/MaterialResolveCS.hlsl/
	* Model/Transparent/ModelTransparentPS.hlsl). morphDeltaOffset_
	* is in float3 units into morphDeltaBufferIndex_; morphVertexOffset_/
	* morphVertexCount_ are this SubMesh's Crister::vertices_ range start and
	* ORIGINAL (pre-LOD) vertex count; morphWeightOffset_ indexes into the
	* shared per-frame morph weight buffer. morphTargetCount_ == 0 disables
	* blending entirely - always the case for a non-morphed instance, and
	* also for a morphed instance drawn from an own-page (streamed-in,
	* non-pool) cluster: only the LOD 0 shared vertex pool range is
	* index-aligned with morphDeltaBufferIndex_/vertexMorphSourceBufferIndex_'s
	* crister-wide numbering (see Crister::vertexMorphSource_'s comment), so
	* ModelRenderer leaves these zeroed for any other cluster.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ラスタのモーフブレンド(モデル用メッシュシェーダーと
	* Model/Material/MaterialResolveCS.hlsl/
	* Model/Transparent/ModelTransparentPS.hlsl 参照)。
	* morphDeltaOffset_ は morphDeltaBufferIndex_ への float3 単位のオフセット。
	* morphVertexOffset_/morphVertexCount_ はこの SubMesh の
	* Crister::vertices_ 範囲の開始位置と元(LOD前)頂点数。
	* morphWeightOffset_ は共有の毎フレームモーフ重みバッファへのインデックス。
	* morphTargetCount_ == 0 でブレンドを完全に無効化する — モーフ無し
	* インスタンスでは常にこの状態。モーフ付きインスタンスでも、自前ページ
	* (ストリームイン済み、プール外)クラスタから描画される場合も同様 —
	* LOD 0 共有頂点プール範囲だけが morphDeltaBufferIndex_/
	* vertexMorphSourceBufferIndex_ の Crister 全体の番号付けと整合するため
	* (Crister::vertexMorphSource_ のコメント参照)、ModelRenderer は
	* それ以外のクラスタではこれらをゼロのままにする。
	*/
	struct ModelMorph
	{
		Uint morphDeltaBufferIndex_;
		Uint vertexMorphSourceBufferIndex_;
		Uint morphDeltaOffset_;
		Uint morphVertexOffset_;

		Uint morphVertexCount_;
		Uint morphTargetCount_;
		Uint morphWeightOffset_;
		Uint modelMorphPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ModelMorph, 32, "Model/Model.hlsli");

	/**
	* [EN]
	* Base PBR values and their matching bindless texture SRVs. ior_ is
	* KHR_materials_ior; emissiveStrength_ is KHR_materials_emissive_strength.
	* All four base textures plus occlusionTextureIndex_ are sampled by
	* Model/Opaque/DeferredLightingPS.hlsl using the model UV recovered from
	* the VisibilityBuffer (RT4.zw) - the deferred pass has no interpolated
	* texcoord of its own.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 基本PBR値と、それに対応する bindless テクスチャ SRV。ior_ は
	* KHR_materials_ior、emissiveStrength_ は KHR_materials_emissive_strength。
	* 基本テクスチャ4種と occlusionTextureIndex_ は
	* Model/Opaque/DeferredLightingPS.hlsl がサンプルする。モデルの UV は
	* VisibilityBuffer(RT4.zw)から取る — deferred パスは自前の補間済み
	* texcoord を持たないため。
	*/
	struct ModelTexture
	{
		Color baseColor_;
		Float metallic_;
		Float roughness_;
		Float alphaCutoff_;
		Float ior_;

		Vector3 emissive_;
		Float emissiveStrength_;

		Uint baseColorTextureIndex_;
		Uint normalTextureIndex_;
		Uint metallicRoughnessTextureIndex_;
		Uint emissiveTextureIndex_;

		Uint occlusionTextureIndex_;
		Vector3 modelTexturePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ModelTexture, 80, "Model/Model.hlsli");

	/**
	* [EN]
	* KHR extension material values/textures (specular, clearcoat,
	* anisotropy, transmission, volume, sheen, iridescence, unlit). Each
	* texture multiplies its matching factor per the glTF spec's channel
	* convention; 0xFFFFFFFF means absent.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* KHR拡張マテリアルの値/テクスチャ(specular, clearcoat, anisotropy,
	* transmission, volume, sheen, iridescence, unlit)。各テクスチャは
	* glTF仕様のチャンネル規約に従って対応するfactorに乗算される。
	* 0xFFFFFFFFは未設定を意味する。
	*/
	struct ModelExtension
	{
		Float specularFactor_;
		Vector3 specularColor_;
		Float clearCoatFactor_;
		Float clearCoatRoughness_;
		Float anisotropy_;
		Float anisotropyRotation_;
		Float transmissionFactor_;
		Float volumeThicknessFactor_;
		Float volumeAttenuationDistance_;
		Float unlit_;
		Vector3 volumeAttenuationColor_;
		Float sheenRoughness_;
		Vector3 sheenColor_;
		Float iridescenceFactor_;
		Float iridescenceIor_;
		Float iridescenceThickness_;
		Uint specularTextureIndex_;
		Uint specularColorTextureIndex_;
		Uint clearCoatTextureIndex_;
		Uint clearCoatRoughnessTextureIndex_;
		Uint clearCoatNormalTextureIndex_;
		Uint transmissionTextureIndex_;
		Uint thicknessTextureIndex_;
		Uint sheenColorTextureIndex_;
		Uint sheenRoughnessTextureIndex_;
		Uint iridescenceTextureIndex_;
		Uint iridescenceThicknessTextureIndex_;
		Uint anisotropyTextureIndex_;
		Vector2 modelExtensionPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ModelExtension, 144, "Model/Model.hlsli");

	/**
	* [EN]
	* Per-instance StructuredBuffer element read by every Model pass (depth
	* prepass, shadow, G-buffer, deferred lighting, GI/reflection/refraction
	* raytracing, OIT). Mirrors Model.hlsli's ModelStructuredBuffer -
	* grouped by concern so the cheap, frequently-run passes (depth/shadow/
	* culling) only ever touch transform_/geometry_/streaming_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全Modelパス(深度プリパス、シャドウ、Gバッファ、ディファードライティング、
	* GI/反射/屈折のレイトレ、OIT)が読む per-instance の StructuredBuffer要素。
	* Model.hlsli の ModelStructuredBuffer と一致 — 関心事ごとにグループ化
	* してあり、軽く頻度の高いパス(深度/シャドウ/カリング)は
	* transform_/geometry_/streaming_ しか触らない。
	*/
	struct ModelStructuredBuffer
	{
		ModelTransform transform_;
		ModelGeometry geometry_;
		ModelStreaming streaming_;
		ModelShading shading_;
		ModelSkining skining_;
		ModelMorph morph_;
		ModelTexture texture_;
		ModelExtension extension_;
	};
	SC_STATIC_ASSERT_SIZE(ModelStructuredBuffer, 576, "Model/Model.hlsli");
}
