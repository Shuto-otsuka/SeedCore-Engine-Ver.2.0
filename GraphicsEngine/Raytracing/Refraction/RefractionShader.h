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
	* Manages RTPSO (state object) creation for the ray-traced refraction pass
	* (RefractionRT.hlsl — raygeneration only, DispatchRays). Same
	* global-root-signature-only shape as ReflectionShader, but there is no
	* miss/closesthit export or hit group: the bounce chain is walked with an
	* inline RayQuery loop inside raygeneration instead of a recursive TraceRay
	* from a closesthit shader (see RefractionRT.hlsl's header comment - no
	* RTPSO in this engine had ever used maxTraceRecursionDepth_ above 1 before,
	* and recursive TraceRay from closesthit reliably failed RTPSO creation),
	* so maxTraceRecursionDepth_ stays 1 like every other RTPSO here.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレ屈折パス(RefractionRT.hlsl — raygenerationのみ、
	* DispatchRays)の RTPSO(ステートオブジェクト)管理。ReflectionShader と
	* 同じグローバルルートシグネチャのみの形だが、miss/closesthitエクスポート
	* もヒットグループも無い: バウンス連鎖は closesthit からの再帰 TraceRay
	* ではなく、raygeneration 内のインライン RayQuery ループで辿る
	* (RefractionRT.hlsl 冒頭コメント参照 - このエンジンで
	* maxTraceRecursionDepth_ を 1 より大きくした RTPSO は無く、closesthit
	* からの再帰 TraceRay は実機で RTPSO 作成が確実に失敗した)ので、
	* maxTraceRecursionDepth_ は他の全 RTPSO と同じく 1 のまま。
	*/
	class RefractionShader
	{
	public:
		RefractionShader(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject);
		~RefractionShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device5* device);

		[[nodiscard]] ID3D12StateObject* GetStateObject()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	public:
		static constexpr const Char* rayGenExportName = "RefractionRayGeneration";

		/// [EN] Enter + exit a single convex refractive shell is 2 bounces; a
		///      few extra allow for a second nested/adjacent surface (e.g. a
		///      glass inside a glass) without immediately going black. Purely
		///      a loop bound for RefractionRT.hlsl's inline RayQuery loop now
		///      (not tied to maxTraceRecursionDepth_, which stays 1).
		/// [JP] 凸形状のガラス1枚の入退場で2バウンス。ネストした/隣接する
		///      2枚目の屈折面(例: グラスの中の氷)があってもすぐ黒くならない
		///      よう、少し余裕を持たせる。RefractionRT.hlsl のインライン
		///      RayQuery ループ回数の上限であり(maxTraceRecursionDepth_ とは
		///      もう無関係、そちらは 1 のまま)。
		static constexpr Uint32 maxBounces_ = 4;

	private:
		Handle<RaytracingShader> libraryShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12StateObject>> stateObjectHandle_;

		Handle<RootSignature> refractionRootSignature_;

		RootSignature& rootSignature_;
		RaytracingStateObject& raytracingStateObject_;
	};
}
