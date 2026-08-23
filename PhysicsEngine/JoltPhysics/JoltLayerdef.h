#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/LayerRegistry.h>
#include <FoundationEngine/ECS/LayerCollisionMatrix.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>

namespace SeedCore
{
	/**
	 * [EN]
	 * Defines the object layers used for collision filtering. A Jolt
	 * ObjectLayer here is a packed value: the low MOTION_TYPE_BITS bits
	 * hold the body's motion-type classification (STATIC/KINEMATIC/
	 * DYNAMIC, below), and the remaining high bits hold the owning
	 * Actor's LayerRegistry slot index (see Pack/UnpackMotionType/
	 * UnpackUserLayer) - so both axes are encoded into the single
	 * value Jolt's broad/narrow phase actually filters on.
	 *
	 * STATIC    : Non-moving geometry (terrain, walls, floors, etc.)
	 * KINEMATIC : Script/animation-driven bodies (moving platforms, doors, etc.)
	 *             Moves but does not respond to physics forces.
	 * DYNAMIC   : Fully physics-simulated bodies (rigidbodies, ragdolls, etc.)
	 *
	 * Motion-type collision matrix (independent of, and applied in
	 * addition to, LayerCollisionMatrix's per-Actor-Layer matrix):
	 *               STATIC  KINEMATIC  DYNAMIC
	 *   STATIC         -        -        o
	 *   KINEMATIC      -        -        o
	 *   DYNAMIC        o        o        o
	 *
	 * ---------------------------------------------------------------------
	 *
	 * [JP]
	 * 衝突フィルタリングに使うオブジェクトレイヤーの定義。ここでの Jolt
	 * ObjectLayer はパックされた値: 下位 MOTION_TYPE_BITS ビットがボディの
	 * 運動タイプ分類（STATIC/KINEMATIC/DYNAMIC、下記）を保持し、残りの
	 * 上位ビットが所有 Actor の LayerRegistry スロットインデックスを保持
	 * する（Pack/UnpackMotionType/UnpackUserLayer 参照）- こうして両方の軸を、
	 * Jolt の Broad/Narrow Phase が実際にフィルタリングに使う単一の値へ
	 * エンコードしている。
	 *
	 * STATIC    : 動かないジオメトリ（地形・壁・床など）
	 * KINEMATIC : スクリプト/アニメーション制御のボディ（動く床・扉など）
	 *             移動するが物理力には反応しない。
	 * DYNAMIC   : 物理演算で完全にシミュレートされるボディ（剛体・ラグドールなど）
	 *
	 * 運動タイプの衝突マトリクス（LayerCollisionMatrix の
	 * Actor レイヤーごとのマトリクスとは独立に、それに加えて適用される）:
	 *               STATIC  KINEMATIC  DYNAMIC
	 *   STATIC         -        -        o
	 *   KINEMATIC      -        -        o
	 *   DYNAMIC        o        o        o
	 */
	namespace Layers
	{
		static constexpr JPH::ObjectLayer STATIC    = 0;
		static constexpr JPH::ObjectLayer KINEMATIC = 1;
		static constexpr JPH::ObjectLayer DYNAMIC   = 2;

		/// [EN] Number of low bits a packed ObjectLayer reserves for the motion type; the remaining high bits hold the Actor's LayerRegistry slot index.
		/// [JP] パックされた ObjectLayer が運動タイプ用に確保する下位ビット数。残りの上位ビットは Actor の LayerRegistry スロットインデックスを保持する。
		static constexpr JPH::uint MOTION_TYPE_BITS = 2;

		/// [EN] Total distinct packed ObjectLayer values Jolt must reserve room for: one motion-type slot per LayerRegistry slot.
		/// [JP] Jolt が確保すべき、パック済み ObjectLayer の総数: LayerRegistry の各スロットにつき1つの運動タイプスロット。
		static constexpr JPH::uint COUNT = static_cast<JPH::uint>(LayerRegistry::LayerCount) << MOTION_TYPE_BITS;

		/**
		* [EN]
		* Packs motionType (STATIC/KINEMATIC/DYNAMIC) and userLayer (a
		* LayerRegistry slot index) into a single Jolt ObjectLayer value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* motionType（STATIC/KINEMATIC/DYNAMIC）と userLayer（LayerRegistry
		* のスロットインデックス）を、単一の Jolt ObjectLayer 値へパックする。
		*/
		constexpr JPH::ObjectLayer Pack(JPH::ObjectLayer motionType, Size userLayer)
		{
			return static_cast<JPH::ObjectLayer>((static_cast<JPH::uint>(userLayer) << MOTION_TYPE_BITS) | motionType);
		}

		/**
		* [EN]
		* Extracts the motion type (STATIC/KINEMATIC/DYNAMIC) packed into
		* objectLayer by Pack.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Pack によって objectLayer へパックされた運動タイプ
		* （STATIC/KINEMATIC/DYNAMIC）を取り出す。
		*/
		constexpr JPH::ObjectLayer UnpackMotionType(JPH::ObjectLayer objectLayer)
		{
			return static_cast<JPH::ObjectLayer>(objectLayer & ((1u << MOTION_TYPE_BITS) - 1u));
		}

		/**
		* [EN]
		* Extracts the LayerRegistry slot index packed into objectLayer by Pack.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Pack によって objectLayer へパックされた LayerRegistry
		* スロットインデックスを取り出す。
		*/
		constexpr Size UnpackUserLayer(JPH::ObjectLayer objectLayer)
		{
			return static_cast<Size>(objectLayer >> MOTION_TYPE_BITS);
		}
	}

	/**
	* [EN]
	* Defines the broad-phase layers used for coarse collision culling.
	* STATIC    and KINEMATIC share one BP layer because neither moves continuously,
	* so Jolt's broad-phase tree does not need to update them separately.
	* DYNAMIC gets its own BP layer so it can be rebuilt every frame efficiently.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 粗い衝突カリングに使うブロードフェーズレイヤーの定義。
	* STATIC と KINEMATIC は同じ BP レイヤーに収める。
	* どちらも連続的には動かないため、ブロードフェーズツリーを別々に更新する必要がない。
	* DYNAMIC は専用の BP レイヤーを持ち、毎フレーム効率よく再構築される。
	*/
	namespace BPLayers
	{
		static constexpr JPH::BroadPhaseLayer STATIC{ 0 };
		static constexpr JPH::BroadPhaseLayer DYNAMIC{ 1 };
		static constexpr JPH::uint            COUNT{ 2 };
	}

	/**
	* [EN]
	* Maps each object layer to its corresponding broad-phase layer.
	* Required by JPH::PhysicsSystem::Init.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 各オブジェクトレイヤーを対応するブロードフェーズレイヤーへマッピングする。
	* JPH::PhysicsSystem::Init に必要。
	*/
	class BPLayerInterfaceImplementation final : public JPH::BroadPhaseLayerInterface
	{
	public:
		JPH::uint GetNumBroadPhaseLayers()const override
		{
			return BPLayers::COUNT;
		}

		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer)const override
		{
			switch (Layers::UnpackMotionType(inLayer))
			{
			case Layers::STATIC:
				[[fallthrough]];
			case Layers::KINEMATIC:
				return BPLayers::STATIC;
			case Layers::DYNAMIC:
				return BPLayers::DYNAMIC;
			default:
				JPH_ASSERT(false);
				return BPLayers::STATIC;
			}
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const Char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer)const override
		{
			switch (static_cast<JPH::uint8>(inLayer))
			{
			case static_cast<JPH::uint8>(BPLayers::STATIC):
				return "STATIC";
			case static_cast<JPH::uint8>(BPLayers::DYNAMIC):
				return "DYNAMIC";
			default:
				return "UNKNOWN";
			}
		}
#endif
	};

	/**
	* [EN]
	* Determines whether an object layer can collide with a broad-phase layer.
	*
	* STATIC    : only needs to test against DYNAMIC BP layer.
	* KINEMATIC : only needs to test against DYNAMIC BP layer.
	* DYNAMIC   : tests against both BP layers.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* オブジェクトレイヤーがブロードフェーズレイヤーと衝突するかを返す。
	*
	* STATIC    : DYNAMIC BP レイヤーとのみ判定すればよい。
	* KINEMATIC : DYNAMIC BP レイヤーとのみ判定すればよい。
	* DYNAMIC   : 両方の BP レイヤーと判定する。
	*/
	class ObjVsBPFilterImplementation final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		Bool ShouldCollide(JPH::ObjectLayer inLayer, JPH::BroadPhaseLayer inBPLayer)const override
		{
			switch (Layers::UnpackMotionType(inLayer))
			{
			case Layers::STATIC:
				[[fallthrough]];
			case Layers::KINEMATIC:
				return inBPLayer == BPLayers::DYNAMIC;
			case Layers::DYNAMIC:
				return true;
			default:
				return false;
			}
		}
	};

	/**
	* [EN]
	* Determines whether two object layers can collide with each other:
	* both the fixed motion-type rules below AND LayerCollisionMatrix's
	* per-Actor-Layer matrix must allow it.
	*
	* STATIC    vs STATIC    : no  (both immovable)
	* STATIC    vs KINEMATIC : no  (Kinematic pushes Dynamic, not Static)
	* STATIC    vs DYNAMIC   : yes
	* KINEMATIC vs KINEMATIC : no  (no physics response between script-driven bodies)
	* KINEMATIC vs DYNAMIC   : yes (Kinematic can push Dynamic)
	* DYNAMIC   vs DYNAMIC   : yes
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つのオブジェクトレイヤーが互いに衝突するかを返す: 下記の固定された
	* 運動タイプルールと、LayerCollisionMatrix の Actor レイヤーごとの
	* マトリクスの両方が許可している必要がある。
	*
	* STATIC    vs STATIC    : しない（どちらも動かない）
	* STATIC    vs KINEMATIC : しない（Kinematic は Dynamic を押すが Static は押さない）
	* STATIC    vs DYNAMIC   : する
	* KINEMATIC vs KINEMATIC : しない（スクリプト制御同士に物理応答は不要）
	* KINEMATIC vs DYNAMIC   : する（Kinematic は Dynamic を押せる）
	* DYNAMIC   vs DYNAMIC   : する
	*/
	class ObjLayerPairFilterImplementation final : public JPH::ObjectLayerPairFilter
	{
	public:
		Bool ShouldCollide(JPH::ObjectLayer inLayerA, JPH::ObjectLayer inLayerB)const override
		{
			JPH::ObjectLayer motionTypeA = Layers::UnpackMotionType(inLayerA);
			JPH::ObjectLayer motionTypeB = Layers::UnpackMotionType(inLayerB);

			Bool motionTypeCollides;
			switch (motionTypeA)
			{
			case Layers::STATIC:
				motionTypeCollides = motionTypeB == Layers::DYNAMIC;
				break;
			case Layers::KINEMATIC:
				motionTypeCollides = motionTypeB == Layers::DYNAMIC;
				break;
			case Layers::DYNAMIC:
				motionTypeCollides = true;
				break;
			default:
				motionTypeCollides = false;
				break;
			}

			if (!motionTypeCollides)
			{
				return false;
			}

			return LayerCollisionMatrix::GetCollide(Layers::UnpackUserLayer(inLayerA), Layers::UnpackUserLayer(inLayerB));
		}
	};

	JPH::EMotionType ToMotionType(Rigidbody::BodyType bodyType);

	/**
	* [EN]
	* Overload of ToObjectLayer that packs userLayer (a LayerRegistry
	* slot index) alongside bodyType's motion type.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* userLayer（LayerRegistry のスロットインデックス）を、bodyType の
	* 運動タイプと合わせてパックする ToObjectLayer のオーバーロード。
	*/
	JPH::ObjectLayer ToObjectLayer(Rigidbody::BodyType bodyType, Size userLayer);

	/**
	* [EN]
	* Overload of ToObjectLayer using LayerRegistry::DefaultLayer, for
	* call sites with no Actor/Layer context available.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actor/Layer の情報が無い呼び出し元向けに、LayerRegistry::DefaultLayer
	* を使う ToObjectLayer のオーバーロード。
	*/
	JPH::ObjectLayer ToObjectLayer(Rigidbody::BodyType bodyType);
}
