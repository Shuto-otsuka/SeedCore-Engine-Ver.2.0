#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <GraphicsEngine/Model/Crister.h>

namespace SeedCore
{
	class Softbody :public SeedScript
	{
	public:
		SC_REFLECTION_CLAMPED_EX("質量", 0.001f, 100.0f)
		Float mass_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("面積弾性力", 0.0f, 1.0f)
		Float areaStiffness_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("体積弾性力", 0.0f, 1.0f)
		Float volumeStiffness_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("抵抗力", 0.0f, 1.0f)
		Float damping_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("ポアソン比", 0.0f, 0.5f)
		Float poissonRatio_ = 0.3f;

		SC_REFLECTION_CLAMPED_EX("最大許容距離", 0.0f, 10.0f)
		Float maxDistance_ = 0.0f;

		SC_REFLECTION_CLAMPED_EX("辺補強係数", 0.0f, 1.0f)
		Float edgeStiffness_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("曲耐性", 0.0f, 1.0f)
		Float bendStiffness_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("内部圧力", -10.0f, 10.0f)
		Float pressure_ = 0.0f;

		SC_REFLECTION_CLAMPED_EX("摩擦係数", 0.0f, 1.0f)
		Float friction_ = 0.2f;

		SC_REFLECTION_CLAMPED_EX("反発係数", 0.0f, 1.0f)
		Float restitution_ = 0.5f;

		SC_REFLECTION_FIELD_EX("重力")
		Bool useGravity_ = true;

		SC_REFLECTION_FIELD_CONDITION(useGravity_)
		SC_REFLECTION_FIELD_EX("重力倍率")
		Float gravityScale_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("サブステップ数", 1, 20)
		Int subSteps_ = 4;

		SC_REFLECTION_CLAMPED_EX("反転回数", 1, 10)
		Int iterationCount_ = 4;

	public:
		void OnAwake();

		void OnFixedTick(Float elapsedTime);

		void OnDestroy();

	public:
		Bool IsPending()const;

		void Build(const Crister& crister);

		/// [EN] True once Build() has produced a live Jolt body (and stays
		///      true until OnDestroy tears it down — e.g. WorldSnapshot::
		///      Restore on Stop). ModelRenderer checks this to decide
		///      whether to render the actor's SoftbodyMesh (deformed) or
		///      fall back to its normal Mesh streaming path (bind pose) —
		///      without this check, stopping Play would leave the last
		///      simulated frame's deformed shape rendered forever, since
		///      SoftbodyMesh::Update() no-ops once vertexPositions_ is
		///      cleared (size no longer matches the bind-pose vertex count).
		/// [JP] Build() が生きた Jolt ボディを作った時点で true になり、
		///      OnDestroy がそれを壊す（例: Stop 時の WorldSnapshot::Restore）
		///      まで true のまま。ModelRenderer がこれを見て、アクターを
		///      SoftbodyMesh（変形後）で描くか、通常の Mesh ストリーミング
		///      経路（バインドポーズ）へフォールバックするかを決める —
		///      このチェックが無いと、Play を止めても最後にシミュレートした
		///      フレームの変形形状が描画され続けてしまう。
		///      SoftbodyMesh::Update() は vertexPositions_ がクリアされた
		///      時点（バインドポーズ頂点数と一致しなくなる）で何もしなく
		///      なるため。
		Bool HasBody()const;

		/// [EN] This frame's simulated vertex positions — of the coarse
		///      simulation proxy (Crister::SoftbodyProxyVertices), not the
		///      full-resolution render mesh (see PhysicsSystem::
		///      ResolveSoftbodies for why). SoftbodyMesh binds the
		///      full-resolution mesh to this proxy at build time and blends
		///      these positions' displacement back onto it every frame.
		/// [JP] このフレームのシミュレート済み頂点位置 — 粗いシミュレーション
		///      用プロキシ（Crister::SoftbodyProxyVertices）のもので、
		///      フル解像度の描画メッシュのものではない（理由は
		///      PhysicsSystem::ResolveSoftbodies 参照）。SoftbodyMesh が
		///      ビルド時にフル解像度メッシュをこのプロキシへ束縛し、毎フレーム
		///      この位置の変位をそこへブレンドし直す。
		const DynamicArray<Vector3>& GetVertexPositions()const;

	private:
		JPH::BodyID bodyID_;

		DynamicArray<Vector3> vertexPositions_;

		Bool pending_ = true;
	};
	REGISTER_COMPONENT(Softbody, "Physics");
}
