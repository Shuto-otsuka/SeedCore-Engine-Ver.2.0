#include <GraphicsEngine/Shader/ShaderCompiler.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/File/FileUtility.h>
#include <FoundationEngine/Log/Error.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	ShaderCompileResult ShaderCompiler::CompileVertexShader(const std::wstring& filePath, const std::string& entryPoint)
	{
		return CompileInternal(String(filePath), String(entryPoint), String("vs_6_6"));
	}

	ShaderCompileResult ShaderCompiler::CompileHullShader(const std::wstring& filePath, const std::string& entryPoint)
	{
		return CompileInternal(String(filePath), String(entryPoint), String("hs_6_6"));
	}

	ShaderCompileResult ShaderCompiler::CompileDomainShader(const std::wstring& filePath, const std::string& entryPoint)
	{
		return CompileInternal(String(filePath), String(entryPoint), String("ds_6_6"));
	}

	ShaderCompileResult ShaderCompiler::CompileGeometryShader(const std::wstring& filePath, const std::string& entryPoint)
	{
		return CompileInternal(String(filePath), String(entryPoint), String("gs_6_6"));
	}

	ShaderCompileResult ShaderCompiler::CompilePixelShader(const std::wstring& filePath, const std::string& entryPoint)
	{
		return CompileInternal(String(filePath), String(entryPoint), String("ps_6_6"));
	}

	ShaderCompileResult ShaderCompiler::CompileAmplificationShader(const std::wstring& filePath, const std::string& entryPoint)
	{
		return CompileInternal(String(filePath), String(entryPoint), String("as_6_6"));
	}

	ShaderCompileResult ShaderCompiler::CompileMeshShader(const std::wstring& filePath, const std::string& entryPoint)
	{
		return CompileInternal(String(filePath), String(entryPoint), String("ms_6_6"));
	}

	ShaderCompileResult ShaderCompiler::CompileComputeShader(const std::wstring& filePath, const std::string& entryPoint)
	{
		return CompileInternal(String(filePath), String(entryPoint), String("cs_6_6"));
	}

	ShaderCompileResult ShaderCompiler::CompileLibraryShader(const std::wstring& filePath)
	{
		return CompileInternal(String(filePath), String("main"), String("lib_6_6"));
	}

	ShaderCompileResult ShaderCompiler::LoadPrecompiled(String filePath)
	{
		HRESULT hr{ S_OK };

		DynamicArray<Uint8> binaryData = FileUtility::LoadFileBinary(filePath);
		if (binaryData.empty())
		{
			return {};
		}

		Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
		hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
		SC_HR_CHECK(hr, "DxcUtilsインスタンスの生成に失敗しました");

		Microsoft::WRL::ComPtr<IDxcBlobEncoding> blobEncoding;
		hr = dxcUtils->CreateBlob(binaryData.data(), static_cast<Uint32>(binaryData.size()), DXC_CP_ACP, &blobEncoding);
		if (FAILED(hr))
		{
			return {};
		}

		ShaderCompileResult result{};
		result.objectBlob = blobEncoding;

		Microsoft::WRL::ComPtr<IDxcContainerReflection> containerReflection;
		hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&containerReflection));
		if (SUCCEEDED(hr))
		{
			hr = containerReflection->Load(blobEncoding.Get());
			if (SUCCEEDED(hr))
			{
				Uint32 partIndex = 0;
				hr = containerReflection->FindFirstPartKind(DXC_PART_REFLECTION_DATA, &partIndex);
				if (SUCCEEDED(hr))
				{
					Microsoft::WRL::ComPtr<IDxcBlob> partBlob;
					containerReflection->GetPartContent(partIndex, &partBlob);
					result.reflectionBlob = partBlob;
				}
			}
		}

		return result;
	}

	String ShaderCompiler::HlslToCsoPath(const std::string& hlslPath)
	{
		auto pos = hlslPath.find_last_of("/\\");
		std::string filename = (pos != std::string::npos) ? hlslPath.substr(pos + 1) : hlslPath;
		filename = filename.substr(0, filename.size() - 5) + ".cso";
		return String("../CompiledShaderObject/" + filename);
	}

	void ShaderCompiler::SaveCso(const String& csoPath, IDxcBlob* blob)
	{
		std::string dir = "../CompiledShaderObject";
		std::filesystem::create_directories(dir);

		std::ofstream ofs(csoPath.str(), std::ios::binary);
		if (ofs)
		{
			ofs.write(static_cast<const Byte*>(blob->GetBufferPointer()), blob->GetBufferSize());
		}
	}

	ShaderCompileResult ShaderCompiler::CompileInternal(String filePath, String entryPoint, String targetProfile)
	{
		std::string path = filePath.str();
		if (path.size() >= 4 && path.substr(path.size() - 4) == ".cso")
		{
			return LoadPrecompiled(filePath);
		}

#ifndef _DEBUG
		String csoPath = HlslToCsoPath(path);
		std::filesystem::path hlslFs(path);
		std::filesystem::path csoFs(csoPath.str());
		if (std::filesystem::exists(csoFs) && std::filesystem::exists(hlslFs))
		{
			auto csoTime = std::filesystem::last_write_time(csoFs);
			auto hlslTime = std::filesystem::last_write_time(hlslFs);
			if (csoTime >= hlslTime)
			{
				ShaderCompileResult cached = LoadPrecompiled(csoPath);
				if (cached.objectBlob)
				{
					return cached;
				}
			}
		}
#endif

		HRESULT hr{ S_OK };

		Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
		hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
		SC_HR_CHECK(hr, "DxcUtilsインスタンスの生成に失敗しました");

		Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
		hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
		SC_HR_CHECK(hr, "DxcCompilerインスタンスの生成に失敗しました");

		Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler;
		hr = dxcUtils->CreateDefaultIncludeHandler(&dxcIncludeHandler);
		SC_HR_CHECK(hr, "IncludeHandlerの生成に失敗しました");

		std::string shaderSource = FileUtility::LoadFileText(filePath);
		if (shaderSource.empty())
		{
			SC_LOG_ERROR("シェーダーソースの読み込みに失敗しました(ファイルが空か見つかりません): {}", filePath.str());
			return {};
		}

		DxcBuffer dxcBuffer{};
		dxcBuffer.Ptr = shaderSource.c_str();
		dxcBuffer.Size = shaderSource.size();
		dxcBuffer.Encoding = DXC_CP_UTF8;

		std::wstring widePath = filePath.w_str();
		std::wstring wideEntry = entryPoint.w_str();
		std::wstring wideProfile = targetProfile.w_str();

#ifdef _DEBUG
		DynamicArray<LPCWSTR> arguments =
		{
			widePath.c_str(),
			L"-E",
			wideEntry.c_str(),
			L"-T",
			wideProfile.c_str(),
			L"-Zi",
			L"-Qembed_debug",
			L"-Od",
		};
#else
		DynamicArray<LPCWSTR> arguments =
		{
			widePath.c_str(),
			L"-E",
			wideEntry.c_str(),
			L"-T",
			wideProfile.c_str(),
			L"-Zi",
			L"-Qstrip_debug",
			L"-O3",
		};
#endif

		Microsoft::WRL::ComPtr<IDxcResult> result;
		hr = dxcCompiler->Compile(&dxcBuffer, arguments.data(), static_cast<Uint32>(arguments.size()), dxcIncludeHandler.Get(), IID_PPV_ARGS(&result));
		SC_HR_CHECK(hr, "コンパイル実行中に致命的なエラーが発生しました");

		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errorBlob;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr);
		if (errorBlob && errorBlob->GetStringLength() > 0)
		{
			SC_LOG_WARNING("シェーダーコンパイル警告: {}", errorBlob->GetStringPointer());
			OutputDebugStringA(errorBlob->GetStringPointer());
		}

		hr = result->GetStatus(&hr);
		if (FAILED(hr))
		{
			SC_LOG_ERROR("シェーダーコンパイル失敗: {} ({})", filePath.str(), targetProfile.str());
			return {};
		}

		ShaderCompileResult compileResult{};
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compileResult.objectBlob), nullptr);
		result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&compileResult.reflectionBlob), nullptr);

#ifndef _DEBUG
		if (compileResult.objectBlob)
		{
			SaveCso(csoPath, compileResult.objectBlob.Get());
		}
#endif

		return compileResult;
	}
}