#include <GraphicsEngine/D3D12/PipelineState/HullShader.h>
#include <GraphicsEngine/Shader/ShaderCompiler.h>

namespace SeedCore
{
	void HullShader::SetBlob(Microsoft::WRL::ComPtr<IDxcBlob> blob, Microsoft::WRL::ComPtr<IDxcBlob> reflectionBlob)
	{
		blob_ = std::move(blob);
		reflectionBlob_ = std::move(reflectionBlob);
	}

	D3D12_SHADER_BYTECODE HullShader::Bytecode()const noexcept
	{
		D3D12_SHADER_BYTECODE bytecode{};
		bytecode.pShaderBytecode = blob_ ? blob_->GetBufferPointer() : nullptr;
		bytecode.BytecodeLength = blob_ ? blob_->GetBufferSize() : 0;
		return bytecode;
	}

	IDxcBlob* HullShader::Blob()const noexcept
	{
		return blob_.Get();
	}

	IDxcBlob* HullShader::ReflectionBlob()const noexcept
	{
		return reflectionBlob_.Get();
	}
}