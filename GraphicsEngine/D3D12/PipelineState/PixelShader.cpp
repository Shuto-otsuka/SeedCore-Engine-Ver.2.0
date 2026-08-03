#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/Shader/ShaderCompiler.h>

namespace SeedCore
{
	void PixelShader::SetBlob(Microsoft::WRL::ComPtr<IDxcBlob> blob, Microsoft::WRL::ComPtr<IDxcBlob> reflectionBlob)
	{
		blob_ = std::move(blob);
		reflectionBlob_ = std::move(reflectionBlob);
	}

	D3D12_SHADER_BYTECODE PixelShader::Bytecode()const noexcept
	{
		D3D12_SHADER_BYTECODE bytecode{};
		bytecode.pShaderBytecode = blob_ ? blob_->GetBufferPointer() : nullptr;
		bytecode.BytecodeLength = blob_ ? blob_->GetBufferSize() : 0;
		return bytecode;
	}

	IDxcBlob* PixelShader::Blob()const noexcept
	{
		return blob_.Get();
	}

	IDxcBlob* PixelShader::ReflectionBlob()const noexcept
	{
		return reflectionBlob_.Get();
	}
}