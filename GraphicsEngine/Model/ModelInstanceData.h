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
		Float instancePadding3_;

		Vector3 specularColor_;
		Float instancePadding4_;

		/// [EN] Dequantisation AABBs for the compressed vertex buffer
		///      (Crister::PositionMin etc.) — see Model.hlsli DecodeModelVertex.
		/// [JP] 圧縮頂点バッファの逆量子化 AABB（Crister::PositionMin ほか）—
		///      Model.hlsli の DecodeModelVertex 参照。
		Vector3 positionMin_;
		Float texcoordMinU_;

		Vector3 positionExtent_;
		Float texcoordMinV_;

		Vector2 texcoordExtent_;
		Vector2 instancePadding5_;
	};
}
