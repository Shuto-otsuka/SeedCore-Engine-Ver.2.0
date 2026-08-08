#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* Local-space (object-space) bounding box: center_ +/- extent_ (extent_
	* is half-extents). Every actor gets a small default-sized Bounds at
	* construction (Actor::Actor) so any actor — including one with no
	* renderable component — can be viewport-picked via a bounding-box
	* raycast. GraphicsEngine's ModelRenderer::Gather overwrites it every
	* frame for actors that resolve a Mesh, from Crister::PositionMin/
	* PositionExtent. Not user-authored (no reflection/serialize fields) and
	* not scene-saved — it is either the fixed default or gets recomputed
	* from the Mesh on load, so persisting it would be redundant. Combined
	* with the actor's own world matrix at query time (see
	* EditorWindowPanel's picking and EditorCamera::FocusOn), this acts as
	* an oriented bounding box without needing to store orientation
	* separately.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ローカル空間（オブジェクト空間）のバウンディングボックス: center_ ±
	* extent_（extent_ は半径に相当する半幅）。全ての actor は構築時
	* （Actor::Actor）に小さいデフォルトサイズの Bounds を持つ — これにより
	* 描画可能なコンポーネントを持たない actor も含め、バウンディング
	* ボックスのレイキャストでビューポートからピック選択できる。
	* GraphicsEngine の ModelRenderer::Gather が、Mesh を解決できた actor に
	* ついては毎フレーム Crister::PositionMin/PositionExtent から上書きする。
	* ユーザーが直接編集するデータではなく（リフレクション/シリアライズ
	* フィールドを持たない）、シーンにも保存しない — 固定のデフォルト値か、
	* ロード後に Mesh から再計算されるかのどちらかなので、永続化すると
	* 冗長になるため。クエリ時にアクター自身のワールド行列と組み合わせる
	* ことで（EditorWindowPanel のピッキングや EditorCamera::FocusOn
	* 参照）、向きを別途保持せずに OBB（有向境界ボックス）として機能する。
	*/
	struct Bounds
	{
		Vector3 center_ = { 0.0f, 0.0f, 0.0f };
		Vector3 extent_ = { 0.5f, 0.5f, 0.5f };
	};
	REGISTER_COMPONENT(Bounds, "Geometry", ComponentStorage::Archetype);
}
