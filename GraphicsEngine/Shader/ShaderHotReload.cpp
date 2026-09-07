#include <GraphicsEngine/Shader/ShaderHotReload.h>
#include <GraphicsEngine/Shader/ShaderCompiler.h>

#include <GraphicsEngine/D3D12/PipelineState/VertexShader.h>
#include <GraphicsEngine/D3D12/PipelineState/HullShader.h>
#include <GraphicsEngine/D3D12/PipelineState/DomainShader.h>
#include <GraphicsEngine/D3D12/PipelineState/GeometryShader.h>
#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/D3D12/PipelineState/AmplificationShader.h>
#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/D3D12/PipelineState/RaytracingShader.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	namespace
	{
		StablePool<VertexShader> vertexShaderPool_;
		StablePool<HullShader> hullShaderPool_;
		StablePool<DomainShader> domainShaderPool_;
		StablePool<GeometryShader> geometryShaderPool_;
		StablePool<PixelShader> pixelShaderPool_;
		StablePool<AmplificationShader> amplificationShaderPool_;
		StablePool<MeshShader> meshShaderPool_;
		StablePool<RaytracingShader> raytracingShaderPool_;
		StablePool<ComputeShader> computeShaderPool_;
	}

	Handle<VertexShader> ShaderHotReload::GetOrCreateVertexShader(String filePath, String entryPoint)
	{
		Key key{};
		key.filePath_ = filePath;
		key.entryPoint_ = entryPoint;

		if (auto found = vertexShaderCache_.find(key))
		{
			return *found;
		}

		ShaderCompileResult compileResult = ShaderCompiler::CompileVertexShader(filePath.w_str(), entryPoint.str());
		errorMessages_[filePath] = compileResult.errorMessage;
		if (!compileResult.objectBlob)
		{
			return Handle<VertexShader>::null();
		}

		Handle<VertexShader> handle = vertexShaderPool_.Create();
		if (auto vertexShader = vertexShaderPool_.Get(handle))
		{
			vertexShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
		}

		vertexShaderCache_.insert(key, handle);
		return handle;
	}

	VertexShader* ShaderHotReload::GetVertexShader(const Handle<VertexShader>& handle)noexcept
	{
		return vertexShaderPool_.Get(handle);
	}

	Handle<HullShader> ShaderHotReload::GetOrCreateHullShader(String filePath, String entryPoint)
	{
		Key key{};
		key.filePath_ = filePath;
		key.entryPoint_ = entryPoint;

		if (auto found = hullShaderCache_.find(key))
		{
			return *found;
		}

		ShaderCompileResult compileResult = ShaderCompiler::CompileHullShader(filePath.w_str(), entryPoint.str());
		errorMessages_[filePath] = compileResult.errorMessage;
		if (!compileResult.objectBlob)
		{
			return Handle<HullShader>::null();
		}

		Handle<HullShader> handle = hullShaderPool_.Create();
		if (auto hullShader = hullShaderPool_.Get(handle))
		{
			hullShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
		}

		hullShaderCache_.insert(key, handle);
		return handle;
	}

	HullShader* ShaderHotReload::GetHullShader(const Handle<HullShader>& handle)noexcept
	{
		return hullShaderPool_.Get(handle);
	}

	Handle<DomainShader> ShaderHotReload::GetOrCreateDomainShader(String filePath, String entryPoint)
	{
		Key key{};
		key.filePath_ = filePath;
		key.entryPoint_ = entryPoint;

		if (auto found = domainShaderCache_.find(key))
		{
			return *found;
		}

		ShaderCompileResult compileResult = ShaderCompiler::CompileDomainShader(filePath.w_str(), entryPoint.str());
		errorMessages_[filePath] = compileResult.errorMessage;
		if (!compileResult.objectBlob)
		{
			return Handle<DomainShader>::null();
		}

		Handle<DomainShader> handle = domainShaderPool_.Create();
		if (auto domainShader = domainShaderPool_.Get(handle))
		{
			domainShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
		}

		domainShaderCache_.insert(key, handle);
		return handle;
	}

	DomainShader* ShaderHotReload::GetDomainShader(const Handle<DomainShader>& handle)noexcept
	{
		return domainShaderPool_.Get(handle);
	}

	Handle<GeometryShader> ShaderHotReload::GetOrCreateGeometryShader(String filePath, String entryPoint)
	{
		Key key{};
		key.filePath_ = filePath;
		key.entryPoint_ = entryPoint;

		if (auto found = geometryShaderCache_.find(key))
		{
			return *found;
		}

		ShaderCompileResult compileResult = ShaderCompiler::CompileGeometryShader(filePath.w_str(), entryPoint.str());
		errorMessages_[filePath] = compileResult.errorMessage;
		if (!compileResult.objectBlob)
		{
			return Handle<GeometryShader>::null();
		}

		Handle<GeometryShader> handle = geometryShaderPool_.Create();
		if (auto geometryShader = geometryShaderPool_.Get(handle))
		{
			geometryShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
		}

		geometryShaderCache_.insert(key, handle);
		return handle;
	}

	GeometryShader* ShaderHotReload::GetGeometryShader(const Handle<GeometryShader>& handle)noexcept
	{
		return geometryShaderPool_.Get(handle);
	}

	Handle<PixelShader> ShaderHotReload::GetOrCreatePixelShader(String filePath, String entryPoint)
	{
		Key key{};
		key.filePath_ = filePath;
		key.entryPoint_ = entryPoint;

		if (auto found = pixelShaderCache_.find(key))
		{
			return *found;
		}

		ShaderCompileResult compileResult = ShaderCompiler::CompilePixelShader(filePath.w_str(), entryPoint.str());
		errorMessages_[filePath] = compileResult.errorMessage;
		if (!compileResult.objectBlob)
		{
			return Handle<PixelShader>::null();
		}

		Handle<PixelShader> handle = pixelShaderPool_.Create();
		if (auto pixelShader = pixelShaderPool_.Get(handle))
		{
			pixelShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
		}

		pixelShaderCache_.insert(key, handle);
		return handle;
	}

	PixelShader* ShaderHotReload::GetPixelShader(const Handle<PixelShader>& handle)noexcept
	{
		return pixelShaderPool_.Get(handle);
	}

	Handle<AmplificationShader> ShaderHotReload::GetOrCreateAmplificationShader(String filePath, String entryPoint)
	{
		Key key{};
		key.filePath_ = filePath;
		key.entryPoint_ = entryPoint;

		if (auto found = amplificationShaderCache_.find(key))
		{
			return *found;
		}

		ShaderCompileResult compileResult = ShaderCompiler::CompileAmplificationShader(filePath.w_str(), entryPoint.str());
		errorMessages_[filePath] = compileResult.errorMessage;
		if (!compileResult.objectBlob)
		{
			return Handle<AmplificationShader>::null();
		}

		Handle<AmplificationShader> handle = amplificationShaderPool_.Create();
		if (auto amplificationShader = amplificationShaderPool_.Get(handle))
		{
			amplificationShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
		}

		amplificationShaderCache_.insert(key, handle);
		return handle;
	}

	AmplificationShader* ShaderHotReload::GetAmplificationShader(const Handle<AmplificationShader>& handle)noexcept
	{
		return amplificationShaderPool_.Get(handle);
	}

	Handle<MeshShader> ShaderHotReload::GetOrCreateMeshShader(String filePath, String entryPoint)
	{
		Key key{};
		key.filePath_ = filePath;
		key.entryPoint_ = entryPoint;

		if (auto found = meshShaderCache_.find(key))
		{
			return *found;
		}

		ShaderCompileResult compileResult = ShaderCompiler::CompileMeshShader(filePath.w_str(), entryPoint.str());
		errorMessages_[filePath] = compileResult.errorMessage;
		if (!compileResult.objectBlob)
		{
			return Handle<MeshShader>::null();
		}

		Handle<MeshShader> handle = meshShaderPool_.Create();
		if (auto meshShader = meshShaderPool_.Get(handle))
		{
			meshShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
		}

		meshShaderCache_.insert(key, handle);
		return handle;
	}

	MeshShader* ShaderHotReload::GetMeshShader(const Handle<MeshShader>& handle)noexcept
	{
		return meshShaderPool_.Get(handle);
	}

	Handle<RaytracingShader> ShaderHotReload::GetOrCreateRaytracingShader(String filePath)
	{
		Key key{};
		key.filePath_ = filePath;

		if (auto found = raytracingShaderCache_.find(key))
		{
			return *found;
		}

		ShaderCompileResult compileResult = ShaderCompiler::CompileLibraryShader(filePath.w_str());
		errorMessages_[filePath] = compileResult.errorMessage;
		if (!compileResult.objectBlob)
		{
			return Handle<RaytracingShader>::null();
		}

		Handle<RaytracingShader> handle = raytracingShaderPool_.Create();
		if (auto raytracingShader = raytracingShaderPool_.Get(handle))
		{
			raytracingShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
		}

		raytracingShaderCache_.insert(key, handle);
		return handle;
	}

	RaytracingShader* ShaderHotReload::GetRaytracingShader(const Handle<RaytracingShader>& handle)noexcept
	{
		return raytracingShaderPool_.Get(handle);
	}

	Handle<ComputeShader> ShaderHotReload::GetOrCreateComputeShader(String filePath, String entryPoint)
	{
		Key key{};
		key.filePath_ = filePath;
		key.entryPoint_ = entryPoint;

		if (auto found = computeShaderCache_.find(key))
		{
			return *found;
		}

		ShaderCompileResult compileResult = ShaderCompiler::CompileComputeShader(filePath.w_str(), entryPoint.str());
		errorMessages_[filePath] = compileResult.errorMessage;
		if (!compileResult.objectBlob)
		{
			return Handle<ComputeShader>::null();
		}

		Handle<ComputeShader> handle = computeShaderPool_.Create();
		if (auto computeShader = computeShaderPool_.Get(handle))
		{
			computeShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
		}

		computeShaderCache_.insert(key, handle);
		return handle;
	}

	ComputeShader* ShaderHotReload::GetComputeShader(const Handle<ComputeShader>& handle)noexcept
	{
		return computeShaderPool_.Get(handle);
	}

	void ShaderHotReload::Reload(String filePath)
	{
		for (auto it = vertexShaderCache_.begin(); it != vertexShaderCache_.end(); ++it)
		{
			if (it->first.filePath_ != filePath)
			{
				continue;
			}
			ShaderCompileResult compileResult = ShaderCompiler::CompileVertexShader(filePath.w_str(), it->first.entryPoint_.str());
			errorMessages_[filePath] = compileResult.errorMessage;
			if (compileResult.objectBlob)
			{
				if (auto vertexShader = vertexShaderPool_.Get(it->second))
				{
					vertexShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
				}
			}
		}

		for (auto it = hullShaderCache_.begin(); it != hullShaderCache_.end(); ++it)
		{
			if (it->first.filePath_ != filePath)
			{
				continue;
			}
			ShaderCompileResult compileResult = ShaderCompiler::CompileHullShader(filePath.w_str(), it->first.entryPoint_.str());
			errorMessages_[filePath] = compileResult.errorMessage;
			if (compileResult.objectBlob)
			{
				if (auto hullShader = hullShaderPool_.Get(it->second))
				{
					hullShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
				}
			}
		}

		for (auto it = domainShaderCache_.begin(); it != domainShaderCache_.end(); ++it)
		{
			if (it->first.filePath_ != filePath)
			{
				continue;
			}
			ShaderCompileResult compileResult = ShaderCompiler::CompileDomainShader(filePath.w_str(), it->first.entryPoint_.str());
			errorMessages_[filePath] = compileResult.errorMessage;
			if (compileResult.objectBlob)
			{
				if (auto domainShader = domainShaderPool_.Get(it->second))
				{
					domainShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
				}
			}
		}

		for (auto it = geometryShaderCache_.begin(); it != geometryShaderCache_.end(); ++it)
		{
			if (it->first.filePath_ != filePath)
			{
				continue;
			}
			ShaderCompileResult compileResult = ShaderCompiler::CompileGeometryShader(filePath.w_str(), it->first.entryPoint_.str());
			errorMessages_[filePath] = compileResult.errorMessage;
			if (compileResult.objectBlob)
			{
				if (auto geometryShader = geometryShaderPool_.Get(it->second))
				{
					geometryShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
				}
			}
		}

		for (auto it = pixelShaderCache_.begin(); it != pixelShaderCache_.end(); ++it)
		{
			if (it->first.filePath_ != filePath)
			{
				continue;
			}
			ShaderCompileResult compileResult = ShaderCompiler::CompilePixelShader(filePath.w_str(), it->first.entryPoint_.str());
			errorMessages_[filePath] = compileResult.errorMessage;
			if (compileResult.objectBlob)
			{
				if (auto pixelShader = pixelShaderPool_.Get(it->second))
				{
					pixelShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
				}
			}
		}

		for (auto it = amplificationShaderCache_.begin(); it != amplificationShaderCache_.end(); ++it)
		{
			if (it->first.filePath_ != filePath)
			{
				continue;
			}
			ShaderCompileResult compileResult = ShaderCompiler::CompileAmplificationShader(filePath.w_str(), it->first.entryPoint_.str());
			errorMessages_[filePath] = compileResult.errorMessage;
			if (compileResult.objectBlob)
			{
				if (auto amplificationShader = amplificationShaderPool_.Get(it->second))
				{
					amplificationShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
				}
			}
		}

		for (auto it = meshShaderCache_.begin(); it != meshShaderCache_.end(); ++it)
		{
			if (it->first.filePath_ != filePath)
			{
				continue;
			}
			ShaderCompileResult compileResult = ShaderCompiler::CompileMeshShader(filePath.w_str(), it->first.entryPoint_.str());
			errorMessages_[filePath] = compileResult.errorMessage;
			if (compileResult.objectBlob)
			{
				if (auto meshShader = meshShaderPool_.Get(it->second))
				{
					meshShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
				}
			}
		}

		for (auto it = raytracingShaderCache_.begin(); it != raytracingShaderCache_.end(); ++it)
		{
			if (it->first.filePath_ != filePath)
			{
				continue;
			}
			ShaderCompileResult compileResult = ShaderCompiler::CompileLibraryShader(filePath.w_str());
			errorMessages_[filePath] = compileResult.errorMessage;
			if (compileResult.objectBlob)
			{
				if (auto raytracingShader = raytracingShaderPool_.Get(it->second))
				{
					raytracingShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
				}
			}
		}

		for (auto it = computeShaderCache_.begin(); it != computeShaderCache_.end(); ++it)
		{
			if (it->first.filePath_ != filePath)
			{
				continue;
			}
			ShaderCompileResult compileResult = ShaderCompiler::CompileComputeShader(filePath.w_str(), it->first.entryPoint_.str());
			errorMessages_[filePath] = compileResult.errorMessage;
			if (compileResult.objectBlob)
			{
				if (auto computeShader = computeShaderPool_.Get(it->second))
				{
					computeShader->SetBlob(compileResult.objectBlob, compileResult.reflectionBlob);
				}
			}
		}
	}

	const std::string& ShaderHotReload::GetErrorMessage(String filePath)const
	{
		static const std::string empty;

		if (auto it = errorMessages_.find(filePath); it != errorMessages_.end())
		{
			return it->second;
		}
		return empty;
	}

	void ShaderHotReload::Clear()noexcept
	{
		for (auto it = vertexShaderCache_.begin(); it != vertexShaderCache_.end(); ++it)
		{
			vertexShaderPool_.Destroy(it->second);
		}
		vertexShaderCache_.clear();

		for (auto it = hullShaderCache_.begin(); it != hullShaderCache_.end(); ++it)
		{
			hullShaderPool_.Destroy(it->second);
		}
		hullShaderCache_.clear();

		for (auto it = domainShaderCache_.begin(); it != domainShaderCache_.end(); ++it)
		{
			domainShaderPool_.Destroy(it->second);
		}
		domainShaderCache_.clear();

		for (auto it = geometryShaderCache_.begin(); it != geometryShaderCache_.end(); ++it)
		{
			geometryShaderPool_.Destroy(it->second);
		}
		geometryShaderCache_.clear();

		for (auto it = pixelShaderCache_.begin(); it != pixelShaderCache_.end(); ++it)
		{
			pixelShaderPool_.Destroy(it->second);
		}
		pixelShaderCache_.clear();

		for (auto it = amplificationShaderCache_.begin(); it != amplificationShaderCache_.end(); ++it)
		{
			amplificationShaderPool_.Destroy(it->second);
		}
		amplificationShaderCache_.clear();

		for (auto it = meshShaderCache_.begin(); it != meshShaderCache_.end(); ++it)
		{
			meshShaderPool_.Destroy(it->second);
		}
		meshShaderCache_.clear();

		for (auto it = raytracingShaderCache_.begin(); it != raytracingShaderCache_.end(); ++it)
		{
			raytracingShaderPool_.Destroy(it->second);
		}
		raytracingShaderCache_.clear();

		for (auto it = computeShaderCache_.begin(); it != computeShaderCache_.end(); ++it)
		{
			computeShaderPool_.Destroy(it->second);
		}
		computeShaderCache_.clear();

		errorMessages_.clear();
	}
}
