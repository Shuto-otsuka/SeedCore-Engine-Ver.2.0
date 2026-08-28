#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class World;
	class Actor;

	/**
	* [EN]
	* Ray-vs-Bounds actor picking, shared by the 3D viewport
	* (EditorWindowPanel) and the 2D canvas (CanvasViewPanel). Every actor
	* has a Bounds (see Bounds's class comment); Pick transforms the ray
	* into each actor's local space via its inverse world matrix so the
	* local-space Bounds acts as an oriented box, and returns the actor
	* whose box the ray enters closest to the origin. Callers build the ray
	* from their own camera (and, for the canvas, first undo the renderer's
	* canvas-space offset/flip so the ray lands in actor world space).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイ対 Bounds のアクターピッキング。3D ビューポート
	* (EditorWindowPanel)と 2D キャンバス(CanvasViewPanel)で共有する。
	* 全アクターは Bounds を持つ(Bounds のクラスコメント参照)。Pick は
	* 各アクターの逆ワールド行列でレイをローカル空間へ変換し、ローカル
	* 空間の Bounds を有向ボックスとして機能させ、レイが起点に最も近く
	* 侵入するボックスのアクターを返す。呼び出し側は自分のカメラから
	* レイを組み立てる(キャンバスの場合はまず、レンダラーのキャンバス
	* 空間オフセット/反転を打ち消し、レイをアクターのワールド空間へ
	* 落とす)。
	*/
	class ViewportPicking
	{
	public:
		/**
		* [EN]
		* Returns the actor whose Bounds the ray hits closest to rayOrigin,
		* or nullptr if it hits none. rayDirection need not be normalized.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* レイが rayOrigin に最も近い位置で当たる Bounds を持つアクターを
		* 返す。どれにも当たらなければ nullptr。rayDirection は正規化
		* されていなくてよい。
		*/
		static Actor* Pick(World& world, const Vector3& rayOrigin, const Vector3& rayDirection);

	private:
		/**
		* [EN]
		* Slab-method ray/AABB test run entirely in the box's own local
		* space (the caller has already transformed the ray there), which
		* doubles as an OBB test for a rotated/scaled actor. outT is the
		* local-space ray parameter to report: the entry point when the ray
		* starts outside the box, or the exit point when it starts inside
		* (so a large enclosing box does not always win as a distance-0 hit).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 箱自身のローカル空間で完全に行うスラブ法のレイ/AABB 判定(呼び
		* 出し側がレイをそこへ変換済み)。回転/スケールしたアクターに対する
		* OBB 判定としても機能する。outT は報告するローカル空間レイ
		* パラメータ: レイが箱の外から始まれば侵入点、内側から始まれば
		* 脱出点(巨大な内包ボックスが距離 0 のヒットとして常に勝たない
		* ように)。
		*/
		static Bool RayIntersectsLocalAABB(const Vector3& rayOriginLocal, const Vector3& rayDirectionLocal, const Vector3& boundsMin, const Vector3& boundsMax, Float& outT);
	};
}
