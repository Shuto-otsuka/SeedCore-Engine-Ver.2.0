#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] Which primitive wireframe a GPU-side collider debug renderer
	///      should expand a ColliderInstance into. Shared between
	///      PhysicsEngine (produces instances from Collider components) and
	///      GraphicsEngine (consumes them for wireframe rendering) — lives in
	///      FoundationEngine, the common base of both, so neither module ever
	///      depends on the other.
	/// [JP] GPU側のコライダーデバッグレンダラーが ColliderInstance を
	///      どのプリミティブのワイヤーフレームへ展開するか。PhysicsEngine
	///      （Collider コンポーネントからインスタンスを生成する側）と
	///      GraphicsEngine（それをワイヤーフレーム描画に使う側）の間で共有
	///      される — 両者の共通基盤である FoundationEngine に置くことで、
	///      どちらのモジュールも互いに依存しない。
	enum class ColliderShapeKind :Uint32
	{
		Box = 0,
		Sphere = 1,
		Capsule = 2,
		Cylinder = 3,
		Rect = 4,
		Circle = 5,
	};

	/// [EN] One collider's worth of GPU-facing debug-draw data: shape kind,
	///      world position/rotation, and shape dimensions (meaning depends on
	///      shapeKind_ — see PhysicsSystem::GatherColliderInstances).
	/// [JP] コライダー1つぶんの、GPU向けデバッグ描画データ: 形状種別、
	///      ワールド位置/回転、形状の寸法（意味は shapeKind_ による —
	///      PhysicsSystem::GatherColliderInstances 参照）。
	struct ColliderInstance
	{
		Vector3 position_;
		Uint32 shapeKind_ = 0;
		Quaternion rotation_ = Quaternion::Identity;
		Vector3 dimensions_;
		Float padding0_ = 0.0f;
		Color color_;
	};
}
