#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class World;
	struct LoaderSystem;
	class AnimationResource;
	class ModelResource;

	/**
	* [EN]
	* Resolves every skinned actor's pose for this frame - clip sampling,
	* cross-state blending, root motion, the node-tree walk and FullBodyIK -
	* and publishes the result onto the actor's Skeleton component. Runs
	* before ConstraintSystem and the renderers so anything downstream
	* (AttachmentConstraint, the inspector's bone list, the bone palette)
	* reads a pose that is current this frame, not one frame stale. A
	* skinned actor with no Skeleton yet gets one here.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このフレームの各スキン付きアクターのポーズを解決する - クリップ
	* サンプリング、ステート間ブレンド、ルートモーション、ノードツリー
	* 走査、FullBodyIK - そして結果をアクターの Skeleton コンポーネントへ
	* 公開する。ConstraintSystem やレンダラより前に走るため、下流
	* (AttachmentConstraint、インスペクタのボーン一覧、ボーンパレット) は
	* 1フレーム遅れではなく当フレームのポーズを読む。Skeleton をまだ
	* 持たないスキン付きアクターにはここで付与する。
	*/
	class AnimationSystem
	{
	public:
		void Execute(World& world, LoaderSystem& loaderSystem, AnimationResource& animationResource, ModelResource& modelResource);
	};
}
