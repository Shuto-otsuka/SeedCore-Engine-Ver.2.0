#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	static FlatMap<BlendStateType, D3D12_BLEND_DESC> blendStateDesc;

	void BlendState::Create()
	{
		{
			D3D12_BLEND_DESC desc{};
			desc.AlphaToCoverageEnable = FALSE;
			desc.IndependentBlendEnable = FALSE;
			desc.RenderTarget[0].BlendEnable = FALSE;
			desc.RenderTarget[0].LogicOpEnable = FALSE;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			blendStateDesc[BlendStateType::Opaque] = desc;
		}

		{
			D3D12_BLEND_DESC desc{};
			desc.AlphaToCoverageEnable = FALSE;
			desc.IndependentBlendEnable = FALSE;
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].LogicOpEnable = FALSE;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			blendStateDesc[BlendStateType::Alpha] = desc;
		}

		{
			D3D12_BLEND_DESC desc{};
			desc.AlphaToCoverageEnable = FALSE;
			desc.IndependentBlendEnable = FALSE;
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].LogicOpEnable = FALSE;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			blendStateDesc[BlendStateType::AlphaPremultiplied] = desc;
		}

		{
			D3D12_BLEND_DESC desc{};
			desc.AlphaToCoverageEnable = FALSE;
			desc.IndependentBlendEnable = FALSE;
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].LogicOpEnable = FALSE;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			blendStateDesc[BlendStateType::Additive] = desc;
		}

		{
			D3D12_BLEND_DESC desc{};
			desc.AlphaToCoverageEnable = FALSE;
			desc.IndependentBlendEnable = FALSE;
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].LogicOpEnable = FALSE;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_DEST_ALPHA;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			blendStateDesc[BlendStateType::Multiply] = desc;
		}

		{
			D3D12_BLEND_DESC desc{};
			desc.AlphaToCoverageEnable = FALSE;
			desc.IndependentBlendEnable = FALSE;
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].LogicOpEnable = FALSE;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			blendStateDesc[BlendStateType::Subtractive] = desc;
		}

		{
			D3D12_BLEND_DESC desc{};
			desc.AlphaToCoverageEnable = FALSE;
			desc.IndependentBlendEnable = FALSE;
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].LogicOpEnable = FALSE;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			blendStateDesc[BlendStateType::Screen] = desc;
		}
	}

	D3D12_BLEND_DESC BlendState::Get(BlendStateType type)
	{
		return blendStateDesc[type];
	}
}
