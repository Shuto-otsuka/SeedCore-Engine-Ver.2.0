#include <GraphicsEngine/Shader/ShaderCompiler.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/File/FileUtility.h>
#include <FoundationEngine/Log/Error.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

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

	ShaderCompileResult ShaderCompiler::CompileInternal(String filePath, String entryPoint, String targetProfile)
	{
		HRESULT hr{ S_OK };

		Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
		hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
		SC_HR_CHECK(hr, "DxcUtilsインスタンスの生成に失敗しました");

		std::string path = filePath.str();
		std::string filename = std::filesystem::path(path).filename().string();
		Bool precompiledOnly = path.size() >= 4 && path.substr(path.size() - 4) == ".cso";

		DynamicArray<Uint8> precompiledData;
		if (precompiledOnly)
		{
			BinaryInputArchive precompiledArchive;
			if (precompiledArchive.Read(filePath))
			{
				precompiledArchive.TryField("data", precompiledData);
			}

			if (precompiledData.empty())
			{
				precompiledData = FileUtility::LoadFileBinary(filePath);
			}
		}

#ifndef _DEBUG
		String csoPath = String("../CompiledShaderObject/Application/" + filename.substr(0, filename.size() - 5) + ".dx.cso");
		String cacheKey = String(targetProfile.str() + ":" + entryPoint.str());
		std::filesystem::path hlslFs(path);
		std::filesystem::path csoFs(csoPath.str());
		std::unordered_map<String, DynamicArray<Uint8>> cachedEntries;
		if (!precompiledOnly && std::filesystem::exists(csoFs))
		{
			Bool sourceNewer = std::filesystem::exists(hlslFs) && std::filesystem::last_write_time(hlslFs) > std::filesystem::last_write_time(csoFs);
			if (!sourceNewer)
			{
				BinaryInputArchive cacheArchive;
				if (cacheArchive.Read(csoPath))
				{
					cacheArchive.TryField("entries", cachedEntries);
				}

				auto cachedEntry = cachedEntries.find(cacheKey);
				if (cachedEntry != cachedEntries.end())
				{
					precompiledData = cachedEntry->second;
				}
			}
		}
#endif

		if (!precompiledData.empty())
		{
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> blobEncoding;
			hr = dxcUtils->CreateBlob(precompiledData.data(), static_cast<Uint32>(precompiledData.size()), DXC_CP_ACP, &blobEncoding);
			if (SUCCEEDED(hr))
			{
				ShaderCompileResult precompiledResult{};
				precompiledResult.objectBlob = blobEncoding;

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
							precompiledResult.reflectionBlob = partBlob;
						}
					}
				}

				return precompiledResult;
			}
		}

		if (precompiledOnly)
		{
			return {};
		}

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

		ShaderCompileResult compileResult{};

		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errorBlob;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr);
		if (errorBlob && errorBlob->GetStringLength() > 0)
		{
			compileResult.errorMessage.assign(errorBlob->GetStringPointer(), errorBlob->GetStringLength());
			SC_LOG_WARNING("シェーダーコンパイル警告: {}", errorBlob->GetStringPointer());
			OutputDebugStringA(errorBlob->GetStringPointer());
		}

		hr = result->GetStatus(&hr);
		if (FAILED(hr))
		{
			SC_LOG_ERROR("シェーダーコンパイル失敗: {} ({})", filePath.str(), targetProfile.str());
			return compileResult;
		}

		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compileResult.objectBlob), nullptr);
		result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&compileResult.reflectionBlob), nullptr);

		if (compileResult.objectBlob)
		{
#ifdef _DEBUG
			std::filesystem::path debugCsoFs("../CompiledShaderObject/Develop/" + filename.substr(0, filename.size() - 5) + ".dbg.cso");
			if (debugCsoFs.has_parent_path())
			{
				std::filesystem::create_directories(debugCsoFs.parent_path());
			}

			std::ofstream debugCsoStream(debugCsoFs, std::ios::binary);
			if (debugCsoStream)
			{
				debugCsoStream.write(static_cast<const Byte*>(compileResult.objectBlob->GetBufferPointer()), compileResult.objectBlob->GetBufferSize());
			}
#else
			if (csoFs.has_parent_path())
			{
				std::filesystem::create_directories(csoFs.parent_path());
			}

			const Uint8* objectBegin = static_cast<const Uint8*>(compileResult.objectBlob->GetBufferPointer());
			cachedEntries[cacheKey] = DynamicArray<Uint8>(objectBegin, objectBegin + compileResult.objectBlob->GetBufferSize());

			BinaryOutputArchive cacheArchive;
			cacheArchive.Field("entries", cachedEntries);
			cacheArchive.Write(csoPath);
#endif
		}

		return compileResult;
	}
}