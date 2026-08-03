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
	* Manages RTPSO (state object) creation for the ray-traced global
	* illumination pass (GlobalIlluminationRT.hlsl — raygen / miss / closesthit,
	* DispatchRays). Same shape as ReflectionShader: the engine's shared root
	* signature is the GLOBAL root signature (bindless table + the two indices
	* CBVs, set via SetComputeRoot* before DispatchRays) and there is no local
	* root signature, because everything is bindless and the hit shader indexes
	* per-instance data by InstanceID().
	*
	* maxTraceRecursionDepth_ stays 1: the bounce surface's sun shadow is fired
	* with an inline RayQuery from closesthit, which does not count against the
	* recursion limit.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレGIパス(GlobalIlluminationRT.hlsl — raygen / miss / closesthit、
	* DispatchRays)の RTPSO(ステートオブジェクト)管理。構成は ReflectionShader
	* と同一で、エンジン共有のルートシグネチャを【グローバル】ルートシグネチャ
	* として使い(bindless テーブル＋2つの indices CBV、DispatchRays 前に
	* SetComputeRoot* で設定)、ローカルルートシグネチャは持たない — 全部
	* bindless で、ヒットシェーダは InstanceID() でインスタンス別データを引く。
	*
	* maxTraceRecursionDepth_ は 1 のまま。バウンス面の太陽影は closesthit から
	* インライン RayQuery で撃っており、そちらは再帰深度の制限対象外。
	*/
	class GlobalIlluminationShader
	{
	public:
		GlobalIlluminationShader(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject);
		~GlobalIlluminationShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device5* device);

		[[nodiscard]] ID3D12StateObject* GetStateObject()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	public:
		static constexpr const Char* rayGenExportName = "GlobalIlluminationRayGeneration";
		static constexpr const Char* missExportName = "GlobalIlluminationMiss";
		static constexpr const Char* closestHitExportName = "GlobalIlluminationClosestHit";
		static constexpr const Char* hitGroupName = "GlobalIlluminationHitGroup";

	private:
		Handle<RaytracingShader> libraryShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12StateObject>> stateObjectHandle_;

		Handle<RootSignature> globalIlluminationRootSignature_;

		RootSignature& rootSignature_;
		RaytracingStateObject& raytracingStateObject_;
	};
}
