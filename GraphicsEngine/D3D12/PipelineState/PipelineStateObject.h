#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
#pragma pack(push, 1)
	struct PipelineStateKey
	{
		ID3D12RootSignature* rootSignature_;

		D3D12_SHADER_BYTECODE vertexShader_;
		D3D12_SHADER_BYTECODE pixelShader_;
		D3D12_SHADER_BYTECODE hullShader_;
		D3D12_SHADER_BYTECODE domainShader_;
		D3D12_SHADER_BYTECODE geometryShader_;

		D3D12_SHADER_BYTECODE amplificationShader_;
		D3D12_SHADER_BYTECODE meshShader_;

		D3D12_SHADER_BYTECODE computeShader_;

		D3D12_RASTERIZER_DESC    rasterizerDesc_;
		D3D12_BLEND_DESC         blendDesc_;
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc_;

		DXGI_FORMAT renderTargetViewFormat_[8];
		UINT        renderTargetViewCount_;

		DXGI_FORMAT depthStencilViewFormat_;

		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType_;
	};
#pragma pack(pop)

	class SEEDCORE_API PipelineStateObject
	{
	private:
		struct PipelineStateKeyHash
		{
			Size operator()(const PipelineStateKey& key)const noexcept
			{
				return std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const Char*>(&key), sizeof(key)));
			}
		};

		struct PipelineStateKeyEqual
		{
			Bool operator()(const PipelineStateKey& lhs, const PipelineStateKey& rhs)const noexcept
			{
				return std::memcmp(&lhs, &rhs, sizeof(PipelineStateKey)) == 0;
			}
		};

	public:
		PipelineStateObject() = default;
		~PipelineStateObject();

		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> GetOrCreate(ID3D12Device* device, const PipelineStateKey& key);

		[[nodiscard]] ID3D12PipelineState* Get(const Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>>& handle);

	private:
		FlatMap<PipelineStateKey, Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>>, PipelineStateKeyHash, PipelineStateKeyEqual> pipelineState_;

		StablePool<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pool_;
	};
}