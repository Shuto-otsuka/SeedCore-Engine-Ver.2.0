#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class ShaderCache;

	/**
	* [EN]
	* Owns the dedicated root signature + PSO for BC7CompressCS.hlsl (used by
	* Crister::BakeBitmap to GPU-compress baked model textures). Built once
	* at startup and shared by every load, like ModelShader.
	*
	* This shader does not use the engine's shared bindless root signature —
	* it binds its input/output as a root SRV/UAV pair instead — so it can't
	* go through the shared RootSignature cache and owns its own signature.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* BC7CompressCS.hlsl（Crister::BakeBitmap がモデルテクスチャの GPU BC7
	* 圧縮に使う）専用のルートシグネチャ+PSO を持つ。ModelShader と同じく
	* 起動時に一度だけ構築し、以降のロード全てで共有する。
	*
	* このシェーダは共有 bindless ルートシグネチャを使わず、入出力を root
	* SRV/UAV で直接バインドするため、共有 RootSignature キャッシュには
	* 乗せられず自前のシグネチャを持つ。
	*/
	class BC7CompressShader
	{
	public:
		BC7CompressShader() = default;
		~BC7CompressShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	};
}
