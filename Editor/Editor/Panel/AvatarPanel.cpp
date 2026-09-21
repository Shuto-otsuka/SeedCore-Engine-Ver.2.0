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
#include <GraphicsEngine/Avatar/Animal/AnimalCharacterConverter.h>
#include <GraphicsEngine/Model/ModelExporter.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12Context.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/File/FileDialog.h>

namespace SeedCore
{
	AvatarPanel::AvatarPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	AvatarPanel::~AvatarPanel()
	{
		ClearRegionTextures();
	}

	void AvatarPanel::Open()
	{
		show_ = true;
		ImGui::SetWindowFocus("アバター生成");
	}

	void AvatarPanel::SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle)
	{
		previewHandle_ = previewHandle;
	}

	Bool AvatarPanel::Focused()const
	{
		return isFocused_;
	}

	void AvatarPanel::EnsureLoaded()
	{
		if (kind_ == AvatarKind::Human)
		{
			if (humanLoadAttempted_)
			{
				return;
			}
			humanLoadAttempted_ = true;

			if (!humanModel_.Load(String("../GraphicsEngine/Avatar/Preset/SeedHuman.hc")))
			{
				return;
			}
			humanEvaluator_.SetModel(humanModel_);
		}
		else
		{
			if (animalLoadAttempted_)
			{
				return;
			}
			animalLoadAttempted_ = true;

			if (!animalModel_.Load(String("../GraphicsEngine/Avatar/Preset/SeedAnimal.ac")))
			{
				return;
			}
			animalEvaluator_.SetModel(animalModel_);
		}
	}

	void AvatarPanel::Bake(ExportPreset preset)
	{
		Bool glb = preset == ExportPreset::Glb;
		Bool gltf = preset == ExportPreset::Gltf;
		const Wchar* extension = gltf ? L"gltf" : glb ? L"glb" : L"fbx";
		std::wstring filter = L"*.";
		filter += extension;

		const Wchar* defaultName = kind_ == AvatarKind::Human ? L"SeedHuman" : L"SeedAnimal";

		std::filesystem::path outputPath;
		if (!FileDialog::SaveFile(outputPath, std::filesystem::current_path(), filter.c_str(), filter.c_str(), extension, defaultName))
		{
			return;
		}

		if (kind_ == AvatarKind::Human)
		{
			humanEvaluator_.Evaluate();
			HumanCharacterConverter::Bake(humanModel_, humanEvaluator_, preset, String(outputPath.string()));
		}
		else
		{
			animalEvaluator_.Evaluate();
			AnimalCharacterConverter::Bake(animalModel_, animalEvaluator_, preset, String(outputPath.string()));
		}
	}

	Uint32 AvatarPanel::ActiveRegionCount()const
	{
		return kind_ == AvatarKind::Human ? humanCharacterRegionCount : animalCharacterRegionCount;
	}

	std::span<const Char* const> AvatarPanel::ActiveRegionLabels()const
	{
		return kind_ == AvatarKind::Human ? HumanCharacterModel::RegionLabels() : AnimalCharacterModel::RegionLabels();
	}

	void AvatarPanel::ClearRegionTextures()
	{
		BindlessHeap* bindlessHeap = context_.graphicsContext_.graphics_->GetBindlessHeap();
		for (Uint32 regionIndex = 0; regionIndex < regionSlotCount_; regionIndex++)
		{
			if (regionTextureIndices_[regionIndex] != 0xFFFFFFFF)
			{
				bindlessHeap->FreeIndex(regionTextureIndices_[regionIndex]);
				regionTextureIndices_[regionIndex] = 0xFFFFFFFF;
			}
			regionTextureResources_[regionIndex].Reset();
			regionTexturePaths_[regionIndex] = String();
		}
	}

	void AvatarPanel::LoadRegionTexture(Uint32 regionIndex, String filePath)
	{
		if (regionIndex >= regionSlotCount_)
		{
			return;
		}
		D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();
		BindlessHeap* bindlessHeap = context_.graphicsContext_.graphics_->GetBindlessHeap();

		if (regionTextureIndices_[regionIndex] == 0xFFFFFFFF)
		{
			regionTextureIndices_[regionIndex] = bindlessHeap->AllocateIndex();
		}
		regionTextureResources_[regionIndex].Reset();
		TextureLoader::CreateTexturePath(d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), bindlessHeap->Heap(), filePath, regionTextureResources_[regionIndex], regionTextureIndices_[regionIndex]);
		regionTexturePaths_[regionIndex] = filePath;
	}

	void AvatarPanel::ExportUvLayout()
	{
		std::span<const Uint32> triangles = kind_ == AvatarKind::Human ? humanModel_.Triangles() : animalModel_.Triangles();
		std::span<const Vector2> texcoords = kind_ == AvatarKind::Human ? humanModel_.Texcoords() : animalModel_.Texcoords();
		if (triangles.empty() || texcoords.empty())
		{
			return;
		}

		std::filesystem::path outputPath;
		if (!FileDialog::SaveFile(outputPath, std::filesystem::current_path(), L"*.png", L"*.png", L"png", kind_ == AvatarKind::Human ? L"SeedHuman_uv" : L"SeedAnimal_uv"))
		{
			return;
		}

		std::span<const Char* const> regionNames = kind_ == AvatarKind::Human ? HumanCharacterModel::RegionNames() : AnimalCharacterModel::RegionNames();
		constexpr Uint32 imageSize = 1024;

		/// [EN] One guide PNG per region, each showing that region's own [0,1] UV
		///      layout - the artist paints one texture file against each.
		/// [JP] リージョンごとにガイド PNG を1枚 - それぞれのリージョン固有の
		///      [0,1] UV 配置を表示する。アーティストは各テクスチャをこれに沿って描く。
		for (Uint32 regionIndex = 0; regionIndex < ActiveRegionCount(); regionIndex++)
		{
			HumanCharacterRegionRange humanRange = kind_ == AvatarKind::Human ? humanModel_.RegionTriangleRange(regionIndex) : HumanCharacterRegionRange{};
			AnimalCharacterRegionRange animalRange = kind_ == AvatarKind::Animal ? animalModel_.RegionTriangleRange(regionIndex) : AnimalCharacterRegionRange{};
			Uint32 firstTriangle = kind_ == AvatarKind::Human ? humanRange.firstTriangle_ : animalRange.firstTriangle_;
			Uint32 triangleCount = kind_ == AvatarKind::Human ? humanRange.triangleCount_ : animalRange.triangleCount_;
			if (triangleCount == 0)
			{
				continue;
			}

			DirectX::ScratchImage image;
			image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, imageSize, imageSize, 1, 1);
			Uint8* pixels = image.GetPixels();
			Size rowPitch = image.GetImage(0, 0, 0)->rowPitch;
			for (Uint32 y = 0; y < imageSize; y++)
			{
				for (Uint32 x = 0; x < imageSize; x++)
				{
					Uint8* pixel = pixels + y * rowPitch + x * 4;
					pixel[0] = 24; pixel[1] = 24; pixel[2] = 28; pixel[3] = 255;
				}
			}

			auto plotLine = [&](Vector2 pointA, Vector2 pointB)
			{
				Int x0 = static_cast<Int>(pointA.x * (imageSize - 1));
				Int y0 = static_cast<Int>((1.0f - pointA.y) * (imageSize - 1));
				Int x1 = static_cast<Int>(pointB.x * (imageSize - 1));
				Int y1 = static_cast<Int>((1.0f - pointB.y) * (imageSize - 1));
				Int deltaX = std::abs(x1 - x0);
				Int deltaY = -std::abs(y1 - y0);
				Int stepX = x0 < x1 ? 1 : -1;
				Int stepY = y0 < y1 ? 1 : -1;
				Int error = deltaX + deltaY;
				while (true)
				{
					if (x0 >= 0 && x0 < static_cast<Int>(imageSize) && y0 >= 0 && y0 < static_cast<Int>(imageSize))
					{
						Uint8* pixel = pixels + y0 * rowPitch + x0 * 4;
						pixel[0] = 210; pixel[1] = 210; pixel[2] = 210; pixel[3] = 255;
					}
					if (x0 == x1 && y0 == y1)
					{
						break;
					}
					Int doubleError = 2 * error;
					if (doubleError >= deltaY)
					{
						error += deltaY; x0 += stepX;
					}
					if (doubleError <= deltaX)
					{
						error += deltaX; y0 += stepY;
					}
				}
			};

			for (Uint32 localTriangle = 0; localTriangle < triangleCount; localTriangle++)
			{
				Size base = static_cast<Size>(firstTriangle + localTriangle) * 3;
				if (base + 2 >= triangles.size())
				{
					break;
				}
				Vector2 a = texcoords[triangles[base + 0]];
				Vector2 b = texcoords[triangles[base + 1]];
				Vector2 c = texcoords[triangles[base + 2]];
				plotLine(a, b);
				plotLine(b, c);
				plotLine(c, a);
			}

			std::filesystem::path regionPath = outputPath;
			std::string suffix = regionIndex < regionNames.size() ? regionNames[regionIndex] : std::to_string(regionIndex);
			regionPath.replace_filename(outputPath.stem().string() + "_" + suffix + outputPath.extension().string());
			std::wstring widePath = regionPath.wstring();
			DirectX::SaveToWICFile(*image.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), widePath.c_str());
		}
	}

	void AvatarPanel::Draw()
	{
		context_.avatarPreviewContext_.previewActive_ = false;
		context_.avatarPreviewContext_.mesh_ = nullptr;
		isFocused_ = false;

		if (!show_)
		{
			return;
		}

		ImGui::DockBuilderDockWindow("アバター生成", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1180, 720), ImGuiCond_FirstUseEver);

		isFocused_ = ImGui::Begin("アバター生成", &show_);
		if (isFocused_)
		{
			const Char* kindNames[] = { "人間", "動物" };
			Int kindIndex = static_cast<Int>(kind_);
			Bool kindChanged = ImGui::Combo("種類", &kindIndex, kindNames, 2);
			if (kindChanged)
			{
				kind_ = static_cast<AvatarKind>(kindIndex);
				ClearRegionTextures();
			}

			EnsureLoaded();

			if (kind_ == AvatarKind::Animal && pendingAnimalGroup_ >= 0)
			{
				if (static_cast<Uint32>(pendingAnimalGroup_) != animalModel_.ActiveGroup())
				{
					animalModel_.SetGroup(static_cast<Uint32>(pendingAnimalGroup_));
					animalEvaluator_.SetModel(animalModel_);
					animalMesh_ = nullptr;
					ClearRegionTextures();
				}
				pendingAnimalGroup_ = -1;
			}

			if ((kindChanged || !cameraReady_) && context_.cameraContext_.avatarCamera_)
			{
				cameraReady_ = true;
				if (kind_ == AvatarKind::Human)
				{
					context_.cameraContext_.avatarCamera_->Focus(Vector3(0.0f, 0.05f, 0.0f));
					context_.cameraContext_.avatarCamera_->Eye(Vector3(0.6f, 0.25f, 2.6f));
				}
				else
				{
					context_.cameraContext_.avatarCamera_->Focus(Vector3(0.0f, 0.45f, 0.0f));
					context_.cameraContext_.avatarCamera_->Eye(Vector3(1.7f, 0.75f, 2.2f));
				}
			}

			Bool loaded = kind_ == AvatarKind::Human ? humanModel_.Loaded() : animalModel_.Loaded();
			if (!loaded)
			{
				ImGui::TextDisabled(kind_ == AvatarKind::Human ? "../GraphicsEngine/Avatar/Preset/SeedHuman.hc を読み込めませんでした" : "../GraphicsEngine/Avatar/Preset/SeedAnimal.ac を読み込めませんでした");
			}
			else
			{
				ResourcePtr<AvatarMesh>& mesh = kind_ == AvatarKind::Human ? humanMesh_ : animalMesh_;
				if (!mesh)
				{
					ID3D12Device* device = context_.graphicsContext_.graphics_->GetContext()->GetDevice();
					BindlessHeap* bindlessHeap = context_.graphicsContext_.graphics_->GetBindlessHeap();
					mesh = MakePtr<AvatarMesh>();
					Uint32 regionRanges[regionSlotCount_ * 2] = {};
					for (Uint32 regionIndex = 0; regionIndex < ActiveRegionCount(); regionIndex++)
					{
						if (kind_ == AvatarKind::Human)
						{
							const HumanCharacterRegionRange& range = humanModel_.RegionTriangleRange(regionIndex);
							regionRanges[regionIndex * 2 + 0] = range.firstTriangle_;
							regionRanges[regionIndex * 2 + 1] = range.triangleCount_;
						}
						else
						{
							const AnimalCharacterRegionRange& range = animalModel_.RegionTriangleRange(regionIndex);
							regionRanges[regionIndex * 2 + 0] = range.firstTriangle_;
							regionRanges[regionIndex * 2 + 1] = range.triangleCount_;
						}
					}
					std::span<const Uint32> regionRangeSpan(regionRanges, ActiveRegionCount() * 2);
					if (kind_ == AvatarKind::Human)
					{
						mesh->Create(device, bindlessHeap, humanModel_.Triangles(), humanModel_.SkinIndices(), humanModel_.SkinWeights(), humanModel_.Texcoords(), humanModel_.Positions(), humanModel_.BodyVertexCount(), regionRangeSpan);
					}
					else
					{
						mesh->Create(device, bindlessHeap, animalModel_.Triangles(), animalModel_.SkinIndices(), animalModel_.SkinWeights(), animalModel_.Texcoords(), animalModel_.Positions(), animalModel_.BodyVertexCount(), regionRangeSpan);
					}
				}

				if (kind_ == AvatarKind::Human)
				{
					humanEvaluator_.Evaluate();
				}
				else
				{
					animalEvaluator_.Evaluate();
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
					if ((orbitHeld || panHeld) && !InputSystem::MouseCaptured())
					{
						InputSystem::BeginMouseCapture();
					}
					context_.cameraContext_.avatarCameraController_->Update(*context_.cameraContext_.avatarCamera_, ImGui::GetIO().DeltaTime);
				}

				if (!orbitHeld && !panHeld && InputSystem::MouseCaptured())
				{
					InputSystem::EndMouseCapture();
				}

				if (mesh->Created())
				{
					context_.avatarPreviewContext_.previewActive_ = true;
					context_.avatarPreviewContext_.mesh_ = &*mesh;
					if (kind_ == AvatarKind::Human)
					{
						context_.avatarPreviewContext_.positions_ = humanEvaluator_.Positions();
						context_.avatarPreviewContext_.normals_ = humanEvaluator_.Normals();
						context_.avatarPreviewContext_.boneCount_ = humanModel_.BoneCount();
					}
					else
					{
						context_.avatarPreviewContext_.positions_ = animalEvaluator_.Positions();
						context_.avatarPreviewContext_.normals_ = animalEvaluator_.Normals();
						context_.avatarPreviewContext_.boneCount_ = animalModel_.BoneCount();
					}
					context_.avatarPreviewContext_.previewWorldMatrix_ = Matrix::Identity;

					context_.avatarPreviewContext_.regionCount_ = ActiveRegionCount();
					for (Uint32 regionIndex = 0; regionIndex < regionSlotCount_; regionIndex++)
					{
						context_.avatarPreviewContext_.regionTextureIndices_[regionIndex] = regionTextureIndices_[regionIndex];
					}
				}
			}
		}
		ImGui::End();
	}

	void AvatarPanel::DrawDetails()
	{
		Bool loaded = kind_ == AvatarKind::Human ? humanModel_.Loaded() : animalModel_.Loaded();
		if (!loaded)
		{
			ImGui::TextDisabled(kind_ == AvatarKind::Human ? "SeedHuman.hc が読み込まれていません" : "SeedAnimal.ac が読み込まれていません");
			return;
		}

		if (ImGui::Button("リセット"))
		{
			if (kind_ == AvatarKind::Human)
			{
				humanEvaluator_.ResetAxisWeights();
			}
			else
			{
				animalEvaluator_.ResetAxisWeights();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("書き出し"))
		{
			ImGui::OpenPopup("avatarBake");
		}
		if (ImGui::BeginPopup("avatarBake"))
		{
			if (ImGui::MenuItem("glTF"))
			{
				Bake(ExportPreset::Gltf);
			}
			if (ImGui::MenuItem("glTF binary"))
			{
				Bake(ExportPreset::Glb);
			}
			if (ImGui::MenuItem("FBX (Maya)"))
			{
				Bake(ExportPreset::FbxMaya);
			}
			if (ImGui::MenuItem("FBX (Unreal)"))
			{
				Bake(ExportPreset::FbxUnreal);
			}
			if (ImGui::MenuItem("FBX (Unity)"))
			{
				Bake(ExportPreset::FbxUnity);
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("テクスチャ", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("UVレイアウト書き出し"))
			{
				ExportUvLayout();
			}
			std::span<const Char* const> regionLabels = ActiveRegionLabels();
			for (Uint32 regionIndex = 0; regionIndex < ActiveRegionCount(); regionIndex++)
			{
				ImGui::PushID(static_cast<Int>(regionIndex));
				const Char* label = regionIndex < regionLabels.size() ? regionLabels[regionIndex] : "region";
				std::string current = regionTexturePaths_[regionIndex].str();
				ImGui::TextUnformatted(label);
				ImGui::SameLine(90.0f);
				ImGui::TextDisabled(current.empty() ? "(なし)" : std::filesystem::path(current).filename().string().c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("設定"))
				{
					std::filesystem::path picked;
					if (FileDialog::OpenFile(picked, std::filesystem::current_path(), L"*.png;*.jpg;*.tga;*.dds", L"*.png"))
					{
						LoadRegionTexture(regionIndex, String(picked.string()));
					}
				}
				if (!current.empty())
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("解除"))
					{
						BindlessHeap* bindlessHeap = context_.graphicsContext_.graphics_->GetBindlessHeap();
						if (regionTextureIndices_[regionIndex] != 0xFFFFFFFF)
						{
							bindlessHeap->FreeIndex(regionTextureIndices_[regionIndex]);
							regionTextureIndices_[regionIndex] = 0xFFFFFFFF;
						}
						regionTextureResources_[regionIndex].Reset();
						regionTexturePaths_[regionIndex] = String();
					}
				}
				ImGui::PopID();
			}
		}

		ImGui::Separator();

		if (kind_ == AvatarKind::Human)
		{
			std::span<const Char* const> axisLabels = HumanCharacterModel::AxisLabels();
			for (Uint32 axisIndex = 0; axisIndex < humanModel_.AxisCount(); axisIndex++)
			{
				const HumanCharacterAxis& axis = humanModel_.Axis(axisIndex);
				const Char* label = axisIndex < axisLabels.size() ? axisLabels[axisIndex] : "axis";
				Float weight = humanEvaluator_.AxisWeight(axisIndex);
				if (ImGui::SliderFloat(label, &weight, axis.minValue_, axis.maxValue_))
				{
					humanEvaluator_.SetAxisWeight(axisIndex, weight);
				}
			}
		}
		else
		{
			std::span<const Char* const> groupNames = AnimalCharacterModel::GroupNames();
			Int groupIndex = pendingAnimalGroup_ >= 0 ? pendingAnimalGroup_ : static_cast<Int>(animalModel_.ActiveGroup());
			if (ImGui::Combo("系統", &groupIndex, groupNames.data(), static_cast<Int>(animalModel_.GroupCount())))
			{
				pendingAnimalGroup_ = groupIndex;
			}

			std::span<const Char* const> axisLabels = AnimalCharacterModel::AxisLabels(animalModel_.ActiveGroup());
			for (Uint32 axisIndex = 0; axisIndex < animalModel_.AxisCount(); axisIndex++)
			{
				const AnimalCharacterAxis& axis = animalModel_.Axis(axisIndex);
				const Char* label = axisIndex < axisLabels.size() ? axisLabels[axisIndex] : "axis";
				Float weight = animalEvaluator_.AxisWeight(axisIndex);
				if (ImGui::SliderFloat(label, &weight, axis.minValue_, axis.maxValue_))
				{
					animalEvaluator_.SetAxisWeight(axisIndex, weight);
				}
			}
		}
	}
}
