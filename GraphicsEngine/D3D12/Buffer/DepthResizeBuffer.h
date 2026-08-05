#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class GeometryBuffer;

	/**
	* [EN]
	* Owns a depth buffer at a DIFFERENT resolution than
	* GeometryBuffer's native one, kept in sync every frame by
	* point-resampling GeometryBuffer's depth via DepthResizeCS.hlsl (see
	* Dispatch). Exists so a fixed-function depth-tested pass can bind a
	* depth buffer matching whatever resolution it is actually rendering
	* at - specifically, the post-tonemap debug overlay (collider
	* wireframes) drawn onto PostProcessRenderer's DLSS Ray
	* Reconstruction-upscaled output resolution, which GeometryBuffer's
	* own native-resolution depth cannot be bound against directly (see
	* Renderer::EndEditorFrame).
	*
	* Backed by two separate resources rather than one dual-purpose one:
	* D3D12 does not allow D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL to be
	* combined with D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS on the same
	* resource (CREATERESOURCE_INVALIDMISCFLAGS). uavResource_ (plain
	* R32_FLOAT, UAV-writable) is what DepthResizeCS.hlsl actually writes
	* into; Dispatch() then CopyResource()s it into depthResource_
	* (R32_TYPELESS with a D32_FLOAT DSV, UAV-incapable) - the two share
	* the same bit layout so the copy is a plain byte-for-byte blit, not a
	* format conversion.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GeometryBufferのネイティブ解像度とは【異なる】解像度の深度バッファを
	* 保持する。毎フレーム、DepthResizeCS.hlsl経由でGeometryBufferの深度を
	* ポイントリサンプルして同期する(Dispatch参照)。固定機能の深度テストを
	* 使うパスが、実際に描画している解像度に一致する深度バッファをバインド
	* できるようにするために存在する - 具体的には、PostProcessRendererの
	* DLSS Ray Reconstriptionアップスケール後出力解像度へ描画する、
	* トーンマップ後デバッグオーバーレイ(コライダーワイヤーフレーム)。
	* GeometryBuffer自身のネイティブ解像度深度は直接バインドできない
	* (Renderer::EndEditorFrame参照)。
	*
	* 1つの兼用リソースではなく、2つの別リソースで構成する: D3D12は
	* D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCILを同一リソース上で
	* D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESSと併用することを許さない
	* (CREATERESOURCE_INVALIDMISCFLAGS)。uavResource_(素のR32_FLOAT、
	* UAV書き込み可能)がDepthResizeCS.hlslが実際に書き込む先。Dispatch()は
	* その後それをdepthResource_(D32_FLOAT DSV付きのR32_TYPELESS、UAV不可)
	* へCopyResource()する - 両者はビットレイアウトが同一なので、この
	* コピーはフォーマット変換ではなく単純なバイト単位のブリットになる。
	*/
	class DepthResizeBuffer
	{
	private:
		struct DepthResizeConstants
		{
			Uint sourceIndex_ = 0;
			Uint destinationIndex_ = 0;
			Uint destinationWidth_ = 0;
			Uint destinationHeight_ = 0;
			Uint sourceWidth_ = 0;
			Uint sourceHeight_ = 0;
			Uint depthResizePadding0_ = 0;
			Uint depthResizePadding1_ = 0;
		};

	public:
		DepthResizeBuffer() = default;
		~DepthResizeBuffer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height);

		/**
		* [EN]
		* Point-resamples geometryBuffer's native-resolution depth into
		* uavResource_ (see DepthResizeCS.hlsl), copies that into
		* depthResource_, then transitions depthResource_ to DEPTH_WRITE -
		* ready for DepthStencilViewHandle() to be bound as a
		* fixed-function depth test target immediately afterward.
		* Internally calls geometryBuffer.EndDepthNonPixel() to make the
		* source depth shader-readable first (same contract as
		* HiZBuffer::Build), so the caller must itself call
		* geometryBuffer.BeginDepth() before its own depth-testing draw -
		* it does not come back from this call already bound.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* geometryBufferのネイティブ解像度深度をuavResource_へポイント
		* リサンプルし(DepthResizeCS.hlsl参照)、それをdepthResource_へ
		* コピーしてからDEPTH_WRITEへ遷移する - 直後にDepthStencilViewHandle()
		* を固定機能の深度テストターゲットとしてバインドできる状態になる。
		* 内部でgeometryBuffer.EndDepthNonPixel()を呼び、先にソース深度を
		* シェーダ読み取り可能にする(HiZBuffer::Buildと同じ契約)。そのため
		* 呼び出し側は、自身の深度テスト描画の前にgeometryBuffer.BeginDepth()
		* を自分で呼ぶ必要がある - この呼び出しから戻った時点でバインド済み
		* にはならない。
		*/
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, GeometryBuffer& geometryBuffer, Uint32 sourceWidth, Uint32 sourceHeight);

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilViewHandle()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> depthResource_;
		D3D12_RESOURCE_STATES depthState_ = D3D12_RESOURCE_STATE_COMMON;

		Microsoft::WRL::ComPtr<ID3D12Resource> uavResource_;
		D3D12_RESOURCE_STATES uavState_ = D3D12_RESOURCE_STATE_COMMON;

		Uint32 width_ = 0;
		Uint32 height_ = 0;

		DescriptorHeap depthStencilViewHeap_;
		Uint32 unorderedAccessViewIndex_ = 0;

		ResourcePtr<ConstantBuffer<DepthResizeConstants>> constantBuffer_;

		BindlessHeap* bindlessHeap_ = nullptr;
		RootSignature* rootSignature_ = nullptr;
		Handle<ComputeShader> resizeShader_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	};
}
