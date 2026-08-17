#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class Crister;
	class Animator;
	struct IKTarget;
	struct JointConstraint;

	/**
	* [EN]
	* Reference:
	* - http://www.andreasaristidou.com/publications/papers/FABRIK.pdf
	* - https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/constraints/TwoBoneIKConstraint.html
	* - http://allenchou.net/2018/05/game-math-swing-twist-interpolation-sterp/
	*
	* Applies Animator::GetIKTarget()'s per-effector targets onto a pose.
	* Each target names only its effector bone - IKPose::ResolveChainRoot
	* walks up parentIndex_ from there to the nearest branch point (a node
	* with more than one child) or the skeleton root.
	*
	* A single active target solves alone (SolveSingle): a 3-node chain
	* (2 bones) with TwoBoneIK's analytic law-of-cosines elbow/knee angle
	* plus an aim rotation, matching the same shape as Unity's
	* TwoBoneIKConstraint; a longer chain with FABRIK's forward/backward
	* position passes, then each bone's rotation recovered from how its
	* direction moved.
	*
	* Two or more simultaneously active targets solve together as one
	* system (SolveGroup, see TreeFABRIK.h): the node tree spanning every
	* active target's nearest common ancestor down to each effector is
	* solved in one TreeFABRIK pass, weighted per target by IKTarget::
	* weight_, so a shared ancestor (e.g. the spine both arms attach to)
	* is pulled toward whichever target's weight_ dominates rather than
	* whichever target happened to be processed last.
	*
	* Either way, every solved joint is then checked against
	* Animator::GetJointConstraint() by bone name: the joint's rotation is
	* split into a swing (deviation from the constraint's axis) and a
	* twist (rotation around it). A Hinge constraint clamps the twist
	* angle to [minAngle_, maxAngle_] and discards the swing; a Cone
	* constraint instead clamps the swing to at most maxAngle_ from the
	* axis. IKPose::ApplyChainPose then writes the (possibly clamped)
	* rotations back into the pose.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Animator::GetIKTarget() の各エフェクタターゲットをポーズへ適用する。
	* 各ターゲットが指定するのはエフェクタのボーンだけ -
	* IKPose::ResolveChainRoot がそこから parentIndex_ を遡り、最初に
	* 見つかる分岐点(子が2つ以上あるノード)またはスケルトンルートまでを
	* 探す。
	*
	* アクティブなターゲットが1つだけの場合は単独で解く(SolveSingle):
	* 3ノード(2ボーン)のチェーンは TwoBoneIK の余弦定理による
	* ひじ/ひざの角度計算とエイム回転の組み合わせで、Unity の
	* TwoBoneIKConstraint と同じ形。それより長いチェーンは FABRIK の
	* forward/backward位置パスで解き、各ボーンの回転はその方向が
	* どう動いたかから復元する。
	*
	* 同時にアクティブなターゲットが2つ以上ある場合は1つの系として
	* まとめて解く(SolveGroup、TreeFABRIK.h参照): 各アクティブターゲットの
	* 最近共通祖先から各エフェクタまでを覆うノードツリーを、
	* IKTarget::weight_ で重み付けして TreeFABRIK で一度に解く。これにより
	* 共有祖先(例: 両腕がぶら下がる背骨)は、処理順で最後に勝った方では
	* なく weight_ が優勢な方のターゲットへ引き寄せられる。
	*
	* いずれの場合も、解いた各関節はその後ボーン名で
	* Animator::GetJointConstraint() と照合される: 関節の回転を、制約の
	* 軸からの逸脱であるswingと、軸周りの回転であるtwistに分解する。
	* Hinge制約はtwist角度を [minAngle_, maxAngle_] にクランプしswingを
	* 捨て、Cone制約は代わりにswingを軸からmaxAngle_以内にクランプする。
	* 最後に IKPose::ApplyChainPose が(クランプ後の)回転をポーズへ
	* 書き戻す。
	*/
	class FullBodyIK
	{
	public:
		static void Apply(const Crister& crister, const Animator& animator, DynamicArray<Matrix>& poseGlobalTransforms);

	private:
		static void SolveSingle(const Crister& crister, Int endNodeIndex, const IKTarget& target, const std::unordered_map<std::string, JointConstraint>& jointConstraints, DynamicArray<Matrix>& poseGlobalTransforms);

		static void SolveGroup(const Crister& crister, const DynamicArray<std::pair<Int, const IKTarget*>>& groupTargets, const std::unordered_map<std::string, JointConstraint>& jointConstraints, DynamicArray<Matrix>& poseGlobalTransforms);
	};
}
