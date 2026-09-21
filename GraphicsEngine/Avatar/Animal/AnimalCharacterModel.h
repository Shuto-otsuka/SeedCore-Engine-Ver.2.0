#pragma once
#include <FoundationEngine/Prelude.h>
namespace SeedCore
{
	constexpr Uint32 animalCharacterMagic = 0x43415353;
	constexpr Uint32 animalCharacterVersion = 3;

	struct AnimalCharacterBone
	{
		Int32 parentIndex_ = -1;
		Float restHead_[3] = { 0.0f, 0.0f, 0.0f };
		Float restTail_[3] = { 0.0f, 0.0f, 0.0f };
	};

	constexpr Uint32 animalCharacterRegionCount = 4;

	struct AnimalCharacterRegionRange
	{
		Uint32 firstTriangle_ = 0;
		Uint32 triangleCount_ = 0;
	};

	struct AnimalCharacterAxis
	{
		Float defaultValue_ = 0.0f;
		Float minValue_ = 0.0f;
		Float maxValue_ = 0.0f;
		DynamicArray<Uint32> vertexIndices_;
		DynamicArray<Vector3> deltas_;
		DynamicArray<Uint32> negativeVertexIndices_;
		DynamicArray<Vector3> negativeDeltas_;
	};

	/**
	* [EN]
	* Loads a .ac parametric-animal asset. The file (format v3) holds several
	* self-contained mesh groups (犬系 / 有蹄系 / ネコ系), each with its own
	* topology, rig and morph axes. SetGroup() picks which one every accessor
	* reads; the leading BlendAxisCount() axes of a group are the body-type
	* blend and the rest are additive feature tweaks.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* .ac パラメトリック動物アセットを読み込む。ファイル(形式v3)は複数の
	* 自己完結メッシュグループ(犬系 / 有蹄系 / ネコ系)を持ち、各々が独自の
	* トポロジ・リグ・モーフ軸を持つ。SetGroup() で全アクセサが読むグループを
	* 選ぶ。グループの先頭 BlendAxisCount() 本が体型ブレンド軸、残りが加算的な
	* 部位調整軸。
	*/
	class SEEDCORE_API AnimalCharacterModel :public NonCopyable
	{
	public:
		AnimalCharacterModel() = default;
		~AnimalCharacterModel() = default;

		Bool Load(String filePath);

		[[nodiscard]] Bool Loaded()const;

		[[nodiscard]] Uint32 GroupCount()const;

		void SetGroup(Uint32 groupIndex);

		[[nodiscard]] Uint32 ActiveGroup()const;

		[[nodiscard]] Uint32 BlendAxisCount()const;

		[[nodiscard]] Uint32 VertexCount()const;

		[[nodiscard]] Uint32 BodyVertexCount()const;

		[[nodiscard]] Uint32 TriangleCount()const;

		[[nodiscard]] Uint32 AxisCount()const;

		[[nodiscard]] Uint32 BoneCount()const;

		[[nodiscard]] std::span<const Vector3> Positions()const;

		[[nodiscard]] std::span<const Vector3> Normals()const;

		[[nodiscard]] std::span<const Vector2> Texcoords()const;

		[[nodiscard]] std::span<const Uint32> Triangles()const;

		[[nodiscard]] std::span<const Uint8> Regions()const;

		[[nodiscard]] std::span<const Float> SkinWeights()const;

		[[nodiscard]] std::span<const Uint32> SkinIndices()const;

		[[nodiscard]] const AnimalCharacterAxis& Axis(Uint32 index)const;

		[[nodiscard]] const AnimalCharacterBone& Bone(Uint32 index)const;

		[[nodiscard]] std::span<const Uint32> JointGroup(Uint32 boneIndex)const;

		[[nodiscard]] const AnimalCharacterRegionRange& RegionTriangleRange(Uint32 regionIndex)const;

		[[nodiscard]] static std::span<const Char* const> GroupNames();

		[[nodiscard]] static std::span<const Char* const> RegionNames();

		[[nodiscard]] static std::span<const Char* const> RegionLabels();

		[[nodiscard]] static std::span<const Char* const> AxisLabels(Uint32 groupIndex);

		[[nodiscard]] static std::span<const Char* const> BoneNames();

	private:
		struct GroupHeader
		{
			Uint32 vertexCount_ = 0;
			Uint32 triangleCount_ = 0;
			Uint32 axisCount_ = 0;
			Uint32 boneCount_ = 0;
			Uint32 blendAxisCount_ = 0;
			Uint32 blockOffsets_[11] = {};
		};

		DynamicArray<Byte> blob_;
		DynamicArray<GroupHeader> groups_;
		Uint32 activeGroup_ = 0;

		Uint32 vertexCount_ = 0;
		Uint32 bodyVertexCount_ = 0;
		Uint32 triangleCount_ = 0;
		Uint32 boneCount_ = 0;
		Uint32 blendAxisCount_ = 0;

		std::span<const Vector3> positions_;
		std::span<const Vector3> normals_;
		std::span<const Vector2> texcoords_;
		std::span<const Uint32> triangles_;
		std::span<const Uint8> regions_;
		std::span<const Float> skinWeights_;
		std::span<const Uint32> skinIndices_;
		std::span<const AnimalCharacterBone> bones_;

		DynamicArray<AnimalCharacterAxis> axes_;
		DynamicArray<DynamicArray<Uint32>> jointGroups_;
		AnimalCharacterRegionRange regionRanges_[animalCharacterRegionCount];
	};
}
