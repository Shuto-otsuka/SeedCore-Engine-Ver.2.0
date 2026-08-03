#include <Editor/Editor/Panel/ControlPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <FoundationEngine/Time/GameTimer.h>
#include <crtdbg.h>

namespace SeedCore
{
	namespace
	{
		_CrtMemState g_playMemCheckpoint;

		void BeginPlayMemCheck()
		{
			_CrtMemCheckpoint(&g_playMemCheckpoint);
		}

		void EndPlayMemCheck()
		{
			_CrtMemState afterState;
			_CrtMemState diffState;
			_CrtMemCheckpoint(&afterState);
			if (_CrtMemDifference(&diffState, &g_playMemCheckpoint, &afterState))
			{
				_CrtMemDumpStatistics(&diffState);
			}
		}
	}

	ControlPanel::ControlPanel(EditorContext& context, ImGuiTexture& imguiTexture) : context_(context), imguiTexture_(imguiTexture)
	{
		/// No Code
	}

	Float ControlPanel::Draw()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		Float menuBarHeight = ImGui::GetFrameHeight();

		Float toolbarHeight = 30.0f;
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, toolbarHeight));

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoScrollWithMouse
			| ImGuiWindowFlags_NoDocking
			| ImGuiWindowFlags_NoSavedSettings;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		if (ImGui::Begin("##ControlPanel", nullptr, flags))
		{
			Bool isPlaying = context_.gameTimer_->IsPlaying();
			Bool isPaused = context_.gameTimer_->IsPaused();

			if (!isPlaying && ImGui::IsKeyPressed(ImGuiKey_F5))
			{
				BeginPlayMemCheck();
				context_.worldSnapshot_.Capture(*context_.world_);
				context_.gameTimer_->Play();
				isPlaying = true;
			}
			if (isPlaying && ImGui::IsKeyPressed(ImGuiKey_F7))
			{
				context_.gameTimer_->Stop();
				context_.worldSnapshot_.Restore(*context_.world_);
				isPlaying = false;
				EndPlayMemCheck();
			}
			if (isPlaying && ImGui::IsKeyPressed(ImGuiKey_F6))
			{
				if (isPaused)
				{
					context_.gameTimer_->Resume();
					isPaused = false;
				}
				else
				{
					context_.gameTimer_->Pause();
					isPaused = true;
				}
			}

			ImVec2 buttonSize(18, 18);
			Float totalWidth = buttonSize.x * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f + ImGui::GetStyle().FramePadding.x * 6.0f;
			Float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
			if (offsetX > 0.0f)
			{
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
			}

			if (isPlaying)
			{
				ImGui::BeginDisabled();
				ImGui::ImageButton("##Play", imguiTexture_.Icon(IconType::Play), buttonSize);
				ImGui::EndDisabled();
			}
			else
			{
				if (ImGui::ImageButton("##Play", imguiTexture_.Icon(IconType::Play), buttonSize))
				{
					BeginPlayMemCheck();
					context_.worldSnapshot_.Capture(*context_.world_);
					context_.gameTimer_->Play();
				}
			}

			ImGui::SameLine();

			if (!isPlaying)
			{
				ImGui::BeginDisabled();
				ImGui::ImageButton("##Pause", imguiTexture_.Icon(IconType::Pause), buttonSize);
				ImGui::EndDisabled();
			}
			else if (isPaused)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
				if (ImGui::ImageButton("##Pause", imguiTexture_.Icon(IconType::Pause), buttonSize))
				{
					context_.gameTimer_->Resume();
				}
				ImGui::PopStyleColor();
			}
			else
			{
				if (ImGui::ImageButton("##Pause", imguiTexture_.Icon(IconType::Pause), buttonSize))
				{
					context_.gameTimer_->Pause();
				}
			}

			ImGui::SameLine();

			if (!isPlaying)
			{
				ImGui::BeginDisabled();
				ImGui::ImageButton("##Stop", imguiTexture_.Icon(IconType::Stop), buttonSize);
				ImGui::EndDisabled();
			}
			else
			{
				if (ImGui::ImageButton("##Stop", imguiTexture_.Icon(IconType::Stop), buttonSize))
				{
					context_.gameTimer_->Stop();
					context_.worldSnapshot_.Restore(*context_.world_);
					EndPlayMemCheck();
				}
			}

		}
		ImGui::End();

		ImGui::PopStyleVar(3);

		return toolbarHeight;
	}
}
