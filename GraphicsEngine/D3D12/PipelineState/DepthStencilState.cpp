#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	static FlatMap<DepthStencilStateType, D3D12_DEPTH_STENCIL_DESC> depthStencilStateDesc;

	void DepthStencilState::Create()
	{
		{
			D3D12_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = FALSE;
			desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.StencilEnable = FALSE;
			desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
			desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			depthStencilStateDesc[DepthStencilStateType::DepthOff] = desc;
		}

		{
			D3D12_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = TRUE;
			desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			desc.StencilEnable = FALSE;
			desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
			desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			depthStencilStateDesc[DepthStencilStateType::DepthOnWriteOn] = desc;
		}

		{
			D3D12_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = TRUE;
			desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			desc.StencilEnable = FALSE;
			desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
			desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			depthStencilStateDesc[DepthStencilStateType::DepthOnWriteOff] = desc;
		}

		{
			D3D12_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = TRUE;
			desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
			desc.StencilEnable = FALSE;
			desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
			desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			depthStencilStateDesc[DepthStencilStateType::DepthOnWriteOnReverseZ] = desc;
		}

		{
			D3D12_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = TRUE;
			desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			desc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
			desc.StencilEnable = FALSE;
			desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
			desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			depthStencilStateDesc[DepthStencilStateType::DepthOnWriteOffReverseZ] = desc;
		}

		{
			D3D12_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = FALSE;
			desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
			desc.StencilEnable = FALSE;
			desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
			desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			depthStencilStateDesc[DepthStencilStateType::DepthOffWriteOnReverseZ] = desc;
		}

		{
			D3D12_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = TRUE;
			desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			desc.StencilEnable = TRUE;
			desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
			desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
			desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
			desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			depthStencilStateDesc[DepthStencilStateType::DepthOnWriteOnStencil] = desc;
		}

		{
			D3D12_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = TRUE;
			desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
			desc.StencilEnable = TRUE;
			desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
			desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
			desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
			desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			depthStencilStateDesc[DepthStencilStateType::DepthOnWriteOnStencilReverseZ] = desc;
		}
	}

	D3D12_DEPTH_STENCIL_DESC DepthStencilState::Get(DepthStencilStateType type)
	{
		return depthStencilStateDesc[type];
	}
}
