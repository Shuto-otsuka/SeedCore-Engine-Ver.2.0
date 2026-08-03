#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct ShaderCompileResult
	{
		Microsoft::WRL::ComPtr<IDxcBlob> objectBlob;
		Microsoft::WRL::ComPtr<IDxcBlob> reflectionBlob;
	};

	class ShaderCompiler
	{
	public:
		static ShaderCompileResult CompileVertexShader(const std::wstring& filePath, const std::string& entryPoint = "main");

		static ShaderCompileResult CompileHullShader(const std::wstring& filePath, const std::string& entryPoint = "main");

		static ShaderCompileResult CompileDomainShader(const std::wstring& filePath, const std::string& entryPoint = "main");

		static ShaderCompileResult CompileGeometryShader(const std::wstring& filePath, const std::string& entryPoint = "main");

		static ShaderCompileResult CompilePixelShader(const std::wstring& filePath, const std::string& entryPoint = "main");

		static ShaderCompileResult CompileAmplificationShader(const std::wstring& filePath, const std::string& entryPoint = "main");

		static ShaderCompileResult CompileMeshShader(const std::wstring& filePath, const std::string& entryPoint = "main");

		static ShaderCompileResult CompileComputeShader(const std::wstring& filePath, const std::string& entryPoint = "main");

		static ShaderCompileResult CompileLibraryShader(const std::wstring& filePath);

	private:
		static ShaderCompileResult CompileInternal(String filePath, String entryPoint, String targetProfile);
		static ShaderCompileResult LoadPrecompiled(String filePath);
		static String HlslToCsoPath(const std::string& hlslPath);
		static void SaveCso(const String& csoPath, IDxcBlob* blob);

	};
}