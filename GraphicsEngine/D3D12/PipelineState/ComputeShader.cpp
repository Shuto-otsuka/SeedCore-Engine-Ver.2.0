#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>
#include <GraphicsEngine/Shader/ShaderCompiler.h>

namespace SeedCore
{
	void ComputeShader::SetBlob(Microsoft::WRL::ComPtr<IDxcBlob> blob, Microsoft::WRL::ComPtr<IDxcBlob> reflectionBlob)
	{
		blob_ = std::move(blob);
		reflectionBlob_ = std::move(reflectionBlob);
	}

	D3D12_SHADER_BYTECODE ComputeShader::Bytecode()const noexcept
	{
		D3D12_SHADER_BYTECODE bytecode{};
		bytecode.pShaderBytecode = blob_ ? blob_->GetBufferPointer() : nullptr;
		bytecode.BytecodeLength = blob_ ? blob_->GetBufferSize() : 0;
		return bytecode;
	}

	IDxcBlob* ComputeShader::Blob()const noexcept
	{
		return blob_.Get();
	}

	IDxcBlob* ComputeShader::ReflectionBlob()const noexcept
	{
		return reflectionBlob_.Get();
	}
}