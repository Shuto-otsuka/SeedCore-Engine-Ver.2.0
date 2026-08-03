#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/Shader/ShaderCompiler.h>

namespace SeedCore
{
	void MeshShader::SetBlob(Microsoft::WRL::ComPtr<IDxcBlob> blob, Microsoft::WRL::ComPtr<IDxcBlob> reflectionBlob)
	{
		blob_ = std::move(blob);
		reflectionBlob_ = std::move(reflectionBlob);
	}

	D3D12_SHADER_BYTECODE MeshShader::Bytecode()const noexcept
	{
		D3D12_SHADER_BYTECODE bytecode{};
		bytecode.pShaderBytecode = blob_ ? blob_->GetBufferPointer() : nullptr;
		bytecode.BytecodeLength = blob_ ? blob_->GetBufferSize() : 0;
		return bytecode;
	}

	IDxcBlob* MeshShader::Blob()const noexcept
	{
		return blob_.Get();
	}

	IDxcBlob* MeshShader::ReflectionBlob()const noexcept
	{
		return reflectionBlob_.Get();
	}
}