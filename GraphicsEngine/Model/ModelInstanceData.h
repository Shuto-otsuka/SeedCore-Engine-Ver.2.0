#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct ModelInstanceData
	{
		Matrix world_;
		Matrix inverseTransposeWorld_;

		/// [EN] This instance's own world matrix as of the previous frame -
		///      see Model.hlsli's ModelInstance::previous_world_.
		/// [JP] このインスタンスの前フレーム時点のワールド行列 —
		///      Model.hlsli の ModelInstance::previous_world_ 参照。
		Matrix previousWorld_;

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

		Uint vertexBufferIndex_;
		Uint meshletBufferIndex_;
		Uint meshletBoundBufferIndex_;
		Uint vertexIndicesBufferIndex_;

		Uint primitiveIndicesBufferIndex_;
		Uint meshletOffset_;
		Uint meshletCount_;
		Uint skinIndex_;

		Uint boneOffset_;
		Uint doubleSided_;
		Uint blend_;
		Uint selected_;

		/// [EN] Cumulative LOD error of this cluster and of the next coarser
		///      cluster in the chain (FLT_MAX = always keep).
		/// [JP] このクラスタの累積 LOD 誤差と、チェーン内の次に粗いクラスタの
		///      誤差（FLT_MAX = 常に描画）。
		Float lodError_;
		Float lodErrorNext_;
		Float specularFactor_;
		Float clearCoatFactor_;

		Float clearCoatRoughness_;
		Float anisotropy_;
		Uint skinVertexBufferIndex_;
		/// [EN] KHR_materials_transmission (factor).
		/// [JP] KHR_materials_transmission(factor)。
		Float transmissionFactor_;

		Vector3 specularColor_;
		/// [EN] KHR_materials_volume (thickness factor).
		/// [JP] KHR_materials_volume(厚みfactor)。
		Float volumeThicknessFactor_;

		/// [EN] Dequantisation AABBs for the compressed vertex buffer
		///      (Crister::PositionMin etc.) — see Model.hlsli DecodeModelVertex.
		/// [JP] 圧縮頂点バッファの逆量子化 AABB（Crister::PositionMin ほか）—
		///      Model.hlsli の DecodeModelVertex 参照。
		Vector3 positionMin_;
		Float texcoordMinU_;

		Vector3 positionExtent_;
		Float texcoordMinV_;

		Vector2 texcoordExtent_;
		/// [EN] KHR_materials_volume (attenuation distance, FLT_MAX = no absorption).
		/// [JP] KHR_materials_volume(吸収距離、FLT_MAX=吸収なし)。
		Float volumeAttenuationDistance_;
		/// [EN] KHR_materials_unlit (1.0 = skip lighting, use base_color+emissive as-is).
		/// [JP] KHR_materials_unlit(1.0=ライティング無効、base_color+emissiveをそのまま使う)。
		Float unlit_;

		/// [EN] KHR_materials_volume (attenuation color, tints light that has
		///      travelled thicknessFactor_ through the medium).
		/// [JP] KHR_materials_volume(吸収色。thicknessFactor_ぶん媒質を通過した
		///      光を着色する)。
		Vector3 volumeAttenuationColor_;
		Float instancePadding6_;

		/// [EN] KHR_materials_sheen.
		/// [JP] KHR_materials_sheen。
		Vector3 sheenColor_;
		Float sheenRoughness_;

		/// [EN] KHR_materials_iridescence.
		/// [JP] KHR_materials_iridescence。
		Float iridescenceFactor_;
		Float iridescenceIor_;
		Float iridescenceThickness_;
		Float instancePadding7_;

		/// [EN] Raster morph blend (see Model.hlsli's ApplyMorphBlend).
		///      morphTargetCount_ == 0 disables blending entirely - always
		///      the case for a non-morphed instance, and also for a
		///      morphed instance drawn from an own-page (streamed-in,
		///      non-pool) cluster: only the LOD 0 shared vertex pool range
		///      is index-aligned with morphDeltaBufferIndex_/
		///      vertexMorphSourceBufferIndex_'s crister-wide numbering (see
		///      Crister::vertexMorphSource_'s comment), so ModelRenderer
		///      leaves these zeroed for any other cluster.
		/// [JP] ラスタのモーフブレンド(Model.hlsli の ApplyMorphBlend 参照)。
		///      morphTargetCount_ == 0 でブレンドを完全に無効化する —
		///      モーフ無しインスタンスでは常にこの状態。モーフ付き
		///      インスタンスでも、自前ページ(ストリームイン済み、プール外)
		///      クラスタから描画される場合も同様 — LOD 0 共有頂点プール
		///      範囲だけが morphDeltaBufferIndex_/
		///      vertexMorphSourceBufferIndex_ の Crister 全体の番号付けと
		///      整合するため(Crister::vertexMorphSource_ のコメント参照)、
		///      ModelRenderer はそれ以外のクラスタではこれらをゼロのまま
		///      にする。
		Uint morphDeltaBufferIndex_;
		Uint vertexMorphSourceBufferIndex_;
		Uint morphDeltaOffset_;
		Uint morphVertexOffset_;

		Uint morphVertexCount_;
		Uint morphTargetCount_;
		Uint morphWeightOffset_;
		Uint instancePadding8_;

		/// [EN] Bindless SRVs of the KHR extension / occlusion textures, or
		///      0xFFFFFFFF when the material has none. Sampled by
		///      Model/Opaque/DeferredLightingPS.hlsl, which gets the model UV from
		///      the VisibilityBuffer (RT4.zw) - the deferred pass has no
		///      interpolated texcoord of its own.
		/// [JP] KHR 拡張 / オクルージョンの各テクスチャの bindless SRV。
		///      マテリアルが持たなければ 0xFFFFFFFF。
		///      Model/Opaque/DeferredLightingPS.hlsl がサンプルする。モデルの UV は
		///      VisibilityBuffer(RT4.zw)から取る — deferred パスは自前の
		///      補間済み texcoord を持たないため。
		Uint occlusionTextureIndex_;
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
		/// [EN] KHR_materials_anisotropy (rotation, radians) - rotates the
		///      tangent frame the anisotropic GGX lobe is stretched along.
		/// [JP] KHR_materials_anisotropy の rotation(ラジアン) — 異方性GGXの
		///      ローブを引き伸ばすタンジェント基底を回転させる。
		Float anisotropyRotation_;
		Uint instancePadding10_;
		Uint instancePadding11_;
	};
}
