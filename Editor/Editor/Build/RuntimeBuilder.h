#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class RuntimeBuilder
	{
	public:
		~RuntimeBuilder();

		void BuildAsync(const std::filesystem::path& projectRoot);

		[[nodiscard]] Bool IsBuilding()const;

		[[nodiscard]] Float GetProgress()const;

		Bool ConsumeResult(Bool& outSuccess, std::string& outLog);

	private:
		Bool Build(const std::filesystem::path& projectRoot, std::string& outLog);

		Bool Package(const std::filesystem::path& projectRoot, const std::filesystem::path& outputDir, std::string& outLog);

		Bool RunProcessCaptureOutput(const std::wstring& commandLine, std::string& outResult, const std::function<void(const std::string&)>& onChunk = nullptr);

		std::optional<std::filesystem::path> FindMSBuild(const std::filesystem::path& projectRoot);

	private:
		std::thread thread_;
		std::atomic<Bool> building_{ false };

		std::mutex resultMutex_;
		Bool hasResult_ = false;
		Bool lastSuccess_ = false;
		std::string lastLog_;

		DynamicArray<Bool> projectSeen_;
		std::atomic<Uint32> completedProjects_{ 0 };
	};
}
