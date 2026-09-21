#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] Wireframe primitive the collider debug renderer expands a ColliderStructuredBuffer entry into.
	/// [JP] コライダーのデバッグ描画が、ColliderStructuredBuffer の1要素を展開するワイヤーフレームの形。
	enum class ColliderShapeKind :Uint32
	{
		Box = 0,
		Sphere = 1,
		Capsule = 2,
		Cylinder = 3,
		Rect = 4,
		Circle = 5,
		Cone = 6,
	};

	/// [EN] One collider's debug-draw data for the GPU; what dimensions_ holds depends on shapeKind_ (see Renderer::GatherColliders).
	/// [JP] コライダー1つ分の、GPU 向けデバッグ描画データ。dimensions_ の中身は shapeKind_ で決まる(Renderer::GatherColliders 参照)。
	struct ColliderStructuredBuffer
	{
		Vector3 position_;
		Uint32 shapeKind_ = 0;
		Quaternion rotation_ = Quaternion::Identity;
		Vector3 dimensions_;
		Float padding0_ = 0.0f;
		Color color_;
	};
}
