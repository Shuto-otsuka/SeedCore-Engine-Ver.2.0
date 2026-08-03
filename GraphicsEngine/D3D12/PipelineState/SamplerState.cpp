#include <GraphicsEngine/D3D12/PipelineState/SamplerState.h>

namespace SeedCore
{
	DynamicArray<D3D12_STATIC_SAMPLER_DESC> SamplerState::Create()
	{
		DynamicArray<D3D12_STATIC_SAMPLER_DESC> samplers;
		samplers.reserve(static_cast<Uint>(SamplerStateType::MAX_COUNT));

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::POINT_WRAP);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::POINT_CLAMP);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::POINT_MIRROR);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::LINEAR_WRAP);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::LINEAR_CLAMP);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::LINEAR_MIRROR);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::ANISOTROPIC_WRAP);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::ANISOTROPIC_CLAMP);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::ANISOTROPIC_MIRROR);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::BORDER_BLACK);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc{};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			samplerDesc.ShaderRegister = static_cast<Uint>(SamplerStateType::BORDER_WHITE);
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

			samplers.push_back(samplerDesc);
		}

		return samplers;
	}
}