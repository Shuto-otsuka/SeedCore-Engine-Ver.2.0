#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Built-in system recomputing every Actor's world-space transform
	* matrix each frame, by walking the scene graph from each root actor
	* down through its children, combining each actor's local
	* Position/Rotation/Scale with its parent's already-computed world matrix.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 各フレームで全 Actor のワールド空間変換行列を再計算する、組み込み
	* システム。各ルート actor からその子を辿ってシーングラフを走査し、
	* 各 actor のローカルな Position/Rotation/Scale を、親の既に計算済みの
	* ワールド行列と組み合わせる。
	*/
	class TransformSystem
	{
	public:
		/**
		* [EN]
		* Recomputes world matrices for every actor in world, starting
		* from each root (parentless) actor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* world 内の全 actor のワールド行列を、各ルート（親を持たない）
		* actor から開始して再計算する。
		*/
		void Execute(World& world);

	private:
		/**
		* [EN]
		* Computes actor's local transform (from its Position/Rotation/
		* Scale components, if present) combined with parentMatrix,
		* stores it as actor's world matrix, then recurses into actor's children.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actor のローカルトランスフォーム（存在すれば Position/
		* Rotation/Scale コンポーネントから）を parentMatrix と組み合わせて
		* 計算し、actor のワールド行列として保存した後、actor の子へ
		* 再帰する。
		*/
		void UpdateActor(class Actor actor, const Matrix& parentMatrix, World& world);
	};
}
