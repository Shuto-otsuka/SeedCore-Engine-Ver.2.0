#include <Editor/Editor/Panel/AvatarPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>
#include <GraphicsEngine/Graphics.h>
#include <GraphicsEngine/Camera/PreviewCamera.h>
#include <GraphicsEngine/Camera/PreviewCameraController.h>
#include <GraphicsEngine/Avatar/AvatarMesh.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterConverter.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/File/FileDialog.h>

namespace SeedCore
{
	AvatarPanel::AvatarPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	AvatarPanel::~AvatarPanel() = default;

	void AvatarPanel::Open()
	{
		show_ = true;
		ImGui::SetWindowFocus("アバター");
	}

	void AvatarPanel::SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle)
	{
		previewHandle_ = previewHandle;
	}

	Bool AvatarPanel::IsFocused()const
	{
		return isFocused_;
	}

	void AvatarPanel::EnsureLoaded()
	{
		if (loadAttempted_)
		{
			return;
		}
		loadAttempted_ = true;

		if (!model_.Load(String("../GraphicsEngine/Avatar/Preset/SeedHuman.hc")))
		{
			return;
		}

		evaluator_.SetModel(model_);

		if (context_.cameraContext_.avatarCamera_)
		{
			context_.cameraContext_.avatarCamera_->Focus(Vector3(0.0f, 0.0f, 0.0f));
			context_.cameraContext_.avatarCamera_->Eye(Vector3(0.0f, 0.1f, -2.8f));
		}
	}

	void AvatarPanel::Bake(Bool binary)
	{
		std::filesystem::path outputPath;
		const Wchar* filterName = binary ? L"glTF binary" : L"glTF";
		const Wchar* filterExt = binary ? L"*.glb" : L"*.gltf";
		const Wchar* defaultExt = binary ? L"glb" : L"gltf";
		if (!FileDialog::SaveFile(outputPath, std::filesystem::current_path(), filterName, filterExt, defaultExt, L"SeedHuman"))
		{
			return;
		}

		evaluator_.Evaluate();

		HumanCharacterConverter::Bake(model_, evaluator_, binary, String(outputPath.string()));
	}

	void AvatarPanel::Draw()
	{
		context_.avatarPreviewContext_.previewActive_ = false;
		context_.avatarPreviewContext_.mesh_ = nullptr;
		context_.avatarPreviewContext_.evaluator_ = nullptr;
		isFocused_ = false;

		if (!show_)
		{
			return;
		}

		ImGui::DockBuilderDockWindow("アバター", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1180, 720), ImGuiCond_FirstUseEver);

		isFocused_ = ImGui::Begin("アバター", &show_);
		if (isFocused_)
		{
			EnsureLoaded();

			if (!model_.IsLoaded())
			{
				ImGui::TextDisabled("../GraphicsEngine/Avatar/Preset/SeedHuman.hc を読み込めませんでした");
			}
			else
			{
				if (!mesh_)
				{
					ID3D12Device* device = context_.graphicsContext_.graphics_->GetContext()->GetDevice();
					BindlessHeap* bindlessHeap = context_.graphicsContext_.graphics_->GetBindlessHeap();
					mesh_ = MakePtr<AvatarMesh>();
					mesh_->Create(device, bindlessHeap, model_);
				}

				ImVec2 previewSize = ImGui::GetContentRegionAvail();
				previewSize.y = Max(previewSize.y, 100.0f);

				if (context_.cameraContext_.avatarCamera_)
				{
					context_.cameraContext_.avatarCamera_->Resize(previewSize.x, previewSize.y);
				}

				ImGui::Image(ImTextureID(previewHandle_.ptr), previewSize);

				Bool orbitHeld = InputSystem::MouseState(InputSystem::MouseButton::Left, InputSystem::IsPressed);
				Bool panHeld = InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed);

				if (ImGui::IsItemHovered() && context_.cameraContext_.avatarCamera_ && context_.cameraContext_.avatarCameraController_)
				{
					if ((orbitHeld || panHeld) && !InputSystem::IsMouseCaptured())
					{
						InputSystem::BeginMouseCapture();
					}
					context_.cameraContext_.avatarCameraController_->Update(*context_.cameraContext_.avatarCamera_, ImGui::GetIO().DeltaTime);
				}

				if (!orbitHeld && !panHeld && InputSystem::IsMouseCaptured())
				{
					InputSystem::EndMouseCapture();
				}

				if (mesh_->IsCreated())
				{
					context_.avatarPreviewContext_.previewActive_ = true;
					context_.avatarPreviewContext_.mesh_ = &*mesh_;
					context_.avatarPreviewContext_.evaluator_ = &evaluator_;
					context_.avatarPreviewContext_.previewWorldMatrix_ = Matrix::Identity;
				}
			}
		}
		ImGui::End();
	}

	void AvatarPanel::DrawDetails()
	{
		if (!model_.IsLoaded())
		{
			ImGui::TextDisabled("SeedHuman.hc が読み込まれていません");
			return;
		}

		if (ImGui::Button("glTF 書き出し"))
		{
			Bake(false);
		}
		ImGui::SameLine();
		if (ImGui::Button("glTF binary 書き出し"))
		{
			Bake(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("リセット"))
		{
			evaluator_.ResetAxisWeights();
		}

		ImGui::Separator();

		std::span<const Char* const> axisLabels = HumanCharacterModel::AxisLabels();
		for (Uint32 axisIndex = 0; axisIndex < model_.AxisCount(); axisIndex++)
		{
			const HumanCharacterAxis& axis = model_.Axis(axisIndex);
			const Char* label = axisIndex < axisLabels.size() ? axisLabels[axisIndex] : "axis";
			Float weight = evaluator_.AxisWeight(axisIndex);
			if (ImGui::SliderFloat(label, &weight, axis.minValue_, axis.maxValue_))
			{
				evaluator_.SetAxisWeight(axisIndex, weight);
			}
		}
	}
}
