#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/RaytracingStateObject.h>

namespace SeedCore
{
	class ShaderCache;
	class RaytracingShader;

	/**
	* [EN]
	* Manages RTPSO (state object) creation for the ray-traced reflection pass
	* (ReflectionRT.hlsl — raygen / miss / closesthit, DispatchRays). Uses the
	* engine's shared root signature as the GLOBAL root signature (bindless
	* table + the two indices CBVs — set via SetComputeRoot* before
	* DispatchRays); no local root signature (everything is bindless, hit
	* shaders index per-instance data by InstanceID()).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレ反射パス(ReflectionRT.hlsl — raygen / miss / closesthit、
	* DispatchRays)の RTPSO(ステートオブジェクト)管理。エンジン共有のルート
	* シグネチャを【グローバル】ルートシグネチャとして使う(bindless テーブル
	* ＋ 2 つの indices CBV — DispatchRays 前に SetComputeRoot* で設定する)。
	* ローカルルートシグネチャは無し(全部 bindless、ヒットシェーダは
	* InstanceID() でインスタンス別データを引く)。
	*/
	class ReflectionShader
	{
	public:
		ReflectionShader(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject);
		~ReflectionShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device5* device);

		[[nodiscard]] ID3D12StateObject* GetStateObject()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	public:
		static constexpr const Char* rayGenExportName = "ReflectionRayGeneration";
		static constexpr const Char* missExportName = "ReflectionMiss";
		static constexpr const Char* closestHitExportName = "ReflectionClosestHit";
		static constexpr const Char* hitGroupName = "ReflectionHitGroup";

	private:
		Handle<RaytracingShader> libraryShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12StateObject>> stateObjectHandle_;

		Handle<RootSignature> reflectionRootSignature_;

		RootSignature& rootSignature_;
		RaytracingStateObject& raytracingStateObject_;
	};
}
