#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class ShaderCache;

	class SkinnedPositionShader
	{
	public:
		SkinnedPositionShader() = default;
		~SkinnedPositionShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	};
}
