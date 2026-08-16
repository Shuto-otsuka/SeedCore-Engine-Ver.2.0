#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct AnimationChannel
	{
		Int sampler_ = -1;
		Int targetNode_ = -1;
		std::string targetPath_;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("sampler", sampler_),
				cereal::make_nvp("target_node", targetNode_),
				cereal::make_nvp("target_path", targetPath_)
			);
		}
	};

	struct AnimationSampler
	{
		Int input_ = -1;
		Int output_ = -1;
		std::string interpolation_;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("input", input_),
				cereal::make_nvp("output", output_),
				cereal::make_nvp("interpolation", interpolation_)
			);
		}
	};

	/// [EN] A single point on the Timeline panel's editor-authored time-remap
	///      curve: value_ is the actual clip sample time to play at when the
	///      ruler reaches time_ (not a multiplier — a direct x-to-y time
	///      mapping, Unity Motion Time / UE5 Sequencer Time Warp style).
	///      time_ == value_ for every point is normal, unmodified speed.
	///      Not part of the glTF-imported skeletal data.
	/// [JP] Timelineパネルでエディタから作成するタイムリマップカーブの1点:
	///      value_はルーラーがtime_に達した時に実際に再生すべきクリップの
	///      サンプル時刻(倍率ではなく、x→yの直接的な時刻マッピング。Unityの
	///      Motion TimeやUE5 SequencerのTime Warpと同じ考え方)。全点で
	///      time_ == value_なら通常速度・無変化。glTFインポート由来の
	///      スケルタルデータではない。
	struct AnimationSpeedKeyframe
	{
		Float time_ = 0.0f;
		Float value_ = 0.0f;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("time", time_),
				cereal::make_nvp("value", value_)
			);
		}
	};

	/// [EN] A single Timeline-panel notify marker (e.g. "play this sound" /
	///      "spawn this VFX" at this point in the clip) — a time + free-text
	///      label only, no value. Consumers resolve label_ themselves.
	/// [JP] Timelineパネルの通知マーカー1つ(例: クリップのこの時刻で
	///      「この効果音を鳴らす」「このVFXを出す」) — 時刻+自由記述ラベルの
	///      みで値は持たない。label_の解釈は利用側に委ねる。
	struct AnimationNotifyEvent
	{
		Float time_ = 0.0f;
		std::string label_;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("time", time_),
				cereal::make_nvp("label", label_)
			);
		}
	};

	class SEEDCORE_API Animation :public NonCopyable
	{
	private:
		friend class AnimationLoader;

		std::string name_;
		Float duration_ = 0.0f;

		DynamicArray<AnimationChannel> channels_;
		DynamicArray<AnimationSampler> samplers_;

		std::unordered_map<Int, DynamicArray<Float>> timelines_;
		std::unordered_map<Int, DynamicArray<Vector3>> scales_;
		std::unordered_map<Int, DynamicArray<Quaternion>> rotations_;
		std::unordered_map<Int, DynamicArray<Vector3>> translations_;

		/// [EN] Morph target weight keyframes for the glTF "weights" target
		///      path, keyed by target node index like timelines_/scales_/
		///      rotations_/translations_ above. Each entry is one row per
		///      timelines_[nodeIndex] keyframe, and each row holds one weight
		///      per morph target on that node's mesh, in the mesh's own
		///      target order (Crister::SubMesh::morphs_ order) — unflattened
		///      at import time in AnimationLoader from glTF's single flat
		///      weights accessor (which packs keyframeCount * targetCount
		///      scalars back to back).
		/// [JP] glTF の "weights" ターゲットパスによるモーフターゲット
		///      ウェイトのキーフレーム。上の timelines_/scales_/rotations_/
		///      translations_ と同じくターゲットノード番号をキーにする。各
		///      エントリは timelines_[nodeIndex] のキーフレームごとに1行、
		///      各行はそのノードのメッシュが持つモーフターゲット1つにつき
		///      1ウェイト(メッシュ自身のターゲット順、Crister::SubMesh::
		///      morphs_ の順序)を保持する — AnimationLoader でのインポート時に
		///      glTF の単一フラットな weights アクセサ(keyframeCount *
		///      targetCount 個のスカラーが連続で詰まっている)から展開される。
		std::unordered_map<Int, DynamicArray<DynamicArray<Float>>> weights_;

		/// [EN] Editor-authored, not glTF-imported — see AnimationSpeedKeyframe
		///      / AnimationNotifyEvent. TimelinePanel edits these directly
		///      through SpeedCurve()/NotifyEvents(); kept unsorted, callers
		///      that need time order sort themselves.
		/// [JP] エディタ作成データでglTFインポートではない — AnimationSpeed
		///      Keyframe / AnimationNotifyEvent参照。TimelinePanelが
		///      SpeedCurve()/NotifyEvents()経由で直接編集する。ソート済みは
		///      保証しない、時刻順が必要な側で各自ソートすること。
		DynamicArray<AnimationSpeedKeyframe> speedCurve_;
		DynamicArray<AnimationNotifyEvent> notifyEvents_;

	public:
		Animation() = default;
		~Animation() = default;

		Animation(Animation&&)noexcept = default;
		Animation& operator=(Animation&&)noexcept = default;

		Float Duration()const;

		void SamplePose(Float time, std::unordered_map<Int, Vector3>& outTranslations, std::unordered_map<Int, Quaternion>& outRotations, std::unordered_map<Int, Vector3>& outScales)const;

		void SampleMorphWeights(Float time, std::unordered_map<Int, DynamicArray<Float>>& outWeights)const;

		Bool HasTranslationChannel(Int nodeIndex)const;

		Int FindRootMotionNode()const;

		[[nodiscard]] DynamicArray<AnimationSpeedKeyframe>& SpeedCurve();
		[[nodiscard]] const DynamicArray<AnimationSpeedKeyframe>& SpeedCurve()const;

		[[nodiscard]] DynamicArray<AnimationNotifyEvent>& NotifyEvents();
		[[nodiscard]] const DynamicArray<AnimationNotifyEvent>& NotifyEvents()const;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("name", name_),
				cereal::make_nvp("duration", duration_),
				cereal::make_nvp("channels", channels_),
				cereal::make_nvp("samplers", samplers_),
				cereal::make_nvp("timelines", timelines_),
				cereal::make_nvp("scales", scales_),
				cereal::make_nvp("rotations", rotations_),
				cereal::make_nvp("translations", translations_),
				cereal::make_nvp("weights", weights_),
				cereal::make_nvp("speed_curve", speedCurve_),
				cereal::make_nvp("notify_events", notifyEvents_)
			);
		}
	};
}
