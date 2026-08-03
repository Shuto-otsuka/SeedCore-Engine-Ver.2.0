#pragma once
#include <FoundationEngine/Prelude.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>

namespace SeedCore
{
	/**
	 * [EN]
	 * Defines the object layers used for collision filtering.
	 *
	 * STATIC    : Non-moving geometry (terrain, walls, floors, etc.)
	 * KINEMATIC : Script/animation-driven bodies (moving platforms, doors, etc.)
	 *             Moves but does not respond to physics forces.
	 * DYNAMIC   : Fully physics-simulated bodies (rigidbodies, ragdolls, etc.)
	 *
	 * Collision matrix:
	 *               STATIC  KINEMATIC  DYNAMIC
	 *   STATIC         -        -        o
	 *   KINEMATIC      -        -        o
	 *   DYNAMIC        o        o        o
	 *
	 * ---------------------------------------------------------------------
	 *
	 * [JP]
	 * 衝突フィルタリングに使うオブジェクトレイヤーの定義。
	 *
	 * STATIC    : 動かないジオメトリ（地形・壁・床など）
	 * KINEMATIC : スクリプト/アニメーション制御のボディ（動く床・扉など）
	 *             移動するが物理力には反応しない。
	 * DYNAMIC   : 物理演算で完全にシミュレートされるボディ（剛体・ラグドールなど）
	 *
	 * 衝突マトリクス:
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
		static constexpr JPH::uint        COUNT     = 3;
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
			switch (inLayer)
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
			switch (inLayer)
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
	* Determines whether two object layers can collide with each other.
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
	* 2つのオブジェクトレイヤーが互いに衝突するかを返す。
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
			switch (inLayerA)
			{
			case Layers::STATIC:
				return inLayerB == Layers::DYNAMIC;
			case Layers::KINEMATIC:
				return inLayerB == Layers::DYNAMIC;
			case Layers::DYNAMIC:
				return true;
			default:
				return false;
			}
		}
	};

	JPH::EMotionType ToMotionType(Rigidbody::BodyType bodyType);

	JPH::ObjectLayer ToObjectLayer(Rigidbody::BodyType bodyType);
}