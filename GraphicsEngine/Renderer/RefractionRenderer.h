#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/Refraction/RefractionShader.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/// [EN] Mirrors Raytracing/Refraction/RefractionRT.hlsl's
	///      RefractionRayConstantBuffer - read by both RefractionRT.hlsl and
	///      DeferredLightingPS.hlsl via structured_indices.refraction_.ray_constant_index_.
	///      Must stay byte-for-byte in sync with the HLSL side.
	/// [JP] Raytracing/Refraction/RefractionRT.hlsl の
	///      RefractionRayConstantBuffer と対応。RefractionRT.hlsl と
	///      DeferredLightingPS.hlsl の両方が
	///      structured_indices.refraction_.ray_constant_index_ 経由で読む。
	///      HLSL 側とバイト単位で一致させること。
	struct RefractionRayConstantBuffer
	{
		Float rayTMax_ = 1000.0f;
		Float normalBias_ = 0.01f;

		/// [EN] Overall refraction intensity applied in DeferredLightingPS.hlsl.
		/// [JP] DeferredLightingPS.hlsl で適用する屈折の全体強度。
		Float strength_ = 1.0f;

		Float refractionPadding_ = 0.0f;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("rayTMax", rayTMax_);
			archive.TryField("normalBias", normalBias_);
			archive.TryField("strength", strength_);
		}
	};

	/**
	* [EN]
	* Owns the ray-traced refraction pass's RTPSO (RefractionShader), its
	* single raw output texture (no denoiser - see RefractionRT.hlsl's header
	* comment for why: the ray is a deterministic Snell-refracted path, not a
	* stochastically importance-sampled one like Reflection's GGX lobe, so
	* there is no per-pixel roughness-driven noise to denoise away), and a
	* 1-record shader table (raygeneration only - global root signature only,
	* no miss/hitgroup export: the bounce chain is an inline RayQuery loop
	* inside raygeneration, see RefractionRT.hlsl). Deliberately does NOT own
	* an instance/material table of its own: RefractionRT.hlsl reuses
	* Reflection's per-triangle tables wholesale (same TLAS, same instance
	* order, same ReflectionMaterialData - see
	* RaytracingRenderer::BuildReflectionMaterialTable), so RaytracingRenderer
	* only needs to call ReflectionRenderer::UpdateInstanceTable once, not once
	* per ray-traced effect.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレ屈折パスの RTPSO(RefractionShader)、単一の生出力テクスチャ
	* (デノイザ無し - 理由は RefractionRT.hlsl 冒頭コメント参照: このレイは
	* Reflection の GGX ローブのような確率的重点サンプリングではなく決定論的な
	* Snell屈折パスなので、デノイズすべき roughness 駆動のピクセル単位ノイズが
	* 無い)、1 レコードのシェーダテーブル(raygenerationのみ —
	* グローバルルートシグネチャのみ、miss/hitgroupエクスポート無し:
	* バウンス連鎖は raygeneration 内のインライン RayQuery ループ、
	* RefractionRT.hlsl 参照)を持つ。あえて自前のインスタンス/
	* マテリアルテーブルは持たない: RefractionRT.hlsl は Reflection の
	* 三角形単位テーブルをそのまま再利用する(同じTLAS、同じ
	* インスタンス順序、同じReflectionMaterialData —
	* RaytracingRenderer::BuildReflectionMaterialTable 参照)ので、
	* RaytracingRenderer は ReflectionRenderer::UpdateInstanceTable を
	* レイトレエフェクトごとにではなく1回呼ぶだけでよい。
	*/
	class RefractionRenderer
	{
	public:
		RefractionRenderer(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject);
		~RefractionRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Updates the tuning constant buffer and registers the output
		///      bindless indices into IndicesSystem. Must run before
		///      IndicesSystem::UploadEditor/UploadGame bakes this frame's indices.
		/// [JP] チューニング用定数バッファを更新し、出力の bindless
		///      インデックスを IndicesSystem へ登録する。
		///      IndicesSystem::UploadEditor/UploadGame が今フレームのインデックスを
		///      確定する前に呼ぶこと。
		void PrepareFrame(const RefractionRayConstantBuffer& settings);

		/// [EN] The actual GPU work: DispatchRays into the output texture (or
		///      clears it to 0 when there is no TLAS / the feature is off / the
		///      RTPSO is missing), leaving it in PIXEL_SHADER_RESOURCE state.
		///      Requires the G-Buffer depth/normal/VisID to already be written.
		/// [JP] 実際の GPU 処理: DispatchRays を出力テクスチャへ(TLAS が無い/
		///      機能が無効/RTPSO が無ければ 0 でクリア)、
		///      PIXEL_SHADER_RESOURCE 状態で終える。G-Buffer の深度/法線/VisID
		///      が書き込み済みであることが前提。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid);

	private:
		RefractionShader refractionShader_;

		ResourcePtr<ConstantBuffer<RefractionRayConstantBuffer>> tuningBuffer_;

		Microsoft::WRL::ComPtr<ID3D12Resource> outputResource_;
		D3D12_RESOURCE_STATES outputState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 outputUnorderedAccessViewIndex_ = 0;
		Uint32 outputShaderResourceViewIndex_ = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> shaderTableResource_;
		static constexpr Uint32 shaderTableRecordSize = 64;

		DescriptorHeap clearHeap_;
		Uint32 clearOutputIndex_ = 0;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Uint32 width_ = 0;
		Uint32 height_ = 0;

		/// [EN] Logs the state-object/shader-table failure once instead of every frame.
		/// [JP] ステートオブジェクト/シェーダテーブル失敗の警告を毎フレームでなく1度だけログ出力する。
		Bool stateObjectMissingLogged_ = false;
	};
}
