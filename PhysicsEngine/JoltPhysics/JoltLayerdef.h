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
	 * DYNAMIC, below), the bits above them hold the owning Actor's
	 * LayerRegistry slot index (see Pack/UnpackMotionType/
	 * UnpackUserLayer), and the topmost bit (PLANAR) marks a 2D canvas
	 * body - so all three axes are encoded into the single value Jolt's
	 * broad/narrow phase actually filters on.
	 *
	 * 2D and 3D bodies share one Jolt world but never interact: a body
	 * only collides with bodies on the same side of the PLANAR bit,
	 * regardless of motion type or LayerCollisionMatrix.
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
	 * 運動タイプ分類（STATIC/KINEMATIC/DYNAMIC、下記）を保持し、その上の
	 * ビットが所有 Actor の LayerRegistry スロットインデックスを保持し
	 * （Pack/UnpackMotionType/UnpackUserLayer 参照）、最上位ビット（PLANAR）が
	 * Canvas の 2D ボディであることを示す - こうして3つの軸すべてを、
	 * Jolt の Broad/Narrow Phase が実際にフィルタリングに使う単一の値へ
	 * エンコードしている。
	 *
	 * 2D と 3D のボディは1つの Jolt ワールドを共有するが、互いに干渉しない:
	 * ボディは PLANAR ビットが同じ側のボディとしか衝突しない。これは
	 * 運動タイプや LayerCollisionMatrix に関係なく常に適用される。
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

		/// [EN] Topmost ObjectLayer bit, set on bodies simulated on the 2D canvas (Rect/CircleCollider). OR'd onto a Pack result; bodies with and without it never collide.
		/// [JP] ObjectLayer の最上位ビット。2D Canvas 上でシミュレートされるボディ（Rect/CircleCollider）に立てる。Pack の結果に OR して使い、このビットの有無が異なるボディ同士は衝突しない。
		static constexpr JPH::ObjectLayer PLANAR = static_cast<JPH::ObjectLayer>(1u << 15);

		/// [EN] Distinct packed ObjectLayer values below the PLANAR bit: one motion-type slot per LayerRegistry slot. A 2D body's layer is one of these with PLANAR added.
		/// [JP] PLANAR ビットより下の、パック済み ObjectLayer の総数: LayerRegistry の各スロットにつき1つの運動タイプスロット。2D ボディのレイヤーは、このいずれかに PLANAR を加えたもの。
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
		JPH::ObjectLayer Pack(JPH::ObjectLayer motionType, Size userLayer);

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
		JPH::ObjectLayer UnpackMotionType(JPH::ObjectLayer objectLayer);

		/**
		* [EN]
		* Extracts the LayerRegistry slot index packed into objectLayer by
		* Pack, ignoring the PLANAR bit.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Pack によって objectLayer へパックされた LayerRegistry
		* スロットインデックスを取り出す。PLANAR ビットは無視する。
		*/
		Size UnpackUserLayer(JPH::ObjectLayer objectLayer);
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
		/**
		* [EN]
		* Returns the number of broad-phase layers exposed to Jolt.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Jolt へ公開するブロードフェーズレイヤー数を返す。
		*/
		JPH::uint GetNumBroadPhaseLayers()const override;

		/**
		* [EN]
		* Maps an object layer's motion type to its broad-phase layer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* オブジェクトレイヤーの運動タイプをブロードフェーズレイヤーへ対応付ける。
		*/
		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer)const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		/**
		* [EN]
		* Returns the profiling name of a broad-phase layer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ブロードフェーズレイヤーのプロファイル表示名を返す。
		*/
		const Char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer)const override;
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
		/**
		* [EN]
		* Determines whether an object layer can collide with a broad-phase layer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* オブジェクトレイヤーがブロードフェーズレイヤーと衝突可能かを返す。
		*/
		Bool ShouldCollide(JPH::ObjectLayer inLayer, JPH::BroadPhaseLayer inBPLayer)const override;
	};

	/**
	* [EN]
	* Determines whether two object layers can collide with each other.
	* Layers on different sides of the PLANAR bit (2D vs 3D) never
	* collide, checked first. Otherwise both the fixed motion-type rules
	* below AND LayerCollisionMatrix's per-Actor-Layer matrix must allow it.
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
	* 2つのオブジェクトレイヤーが互いに衝突するかを返す。PLANAR ビットが
	* 異なるレイヤー同士（2D と 3D）は決して衝突せず、これを最初に判定する。
	* それ以外は、下記の固定された運動タイプルールと、LayerCollisionMatrix の
	* Actor レイヤーごとのマトリクスの両方が許可している必要がある。
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
		/**
		* [EN]
		* Determines whether two packed object layers can collide.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* パックされた2つのオブジェクトレイヤーが衝突可能かを返す。
		*/
		Bool ShouldCollide(JPH::ObjectLayer inLayerA, JPH::ObjectLayer inLayerB)const override;
	};

	/**
	* [EN]
	* Converts a rigid-body type to the corresponding Jolt motion type.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Rigidbody の種類を対応する Jolt 運動タイプへ変換する。
	*/
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
