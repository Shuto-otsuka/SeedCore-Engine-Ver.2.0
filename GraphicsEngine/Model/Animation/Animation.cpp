#include <GraphicsEngine/Model/Animation/Animation.h>

namespace SeedCore
{
	Float Animation::Duration()const
	{
		return duration_;
	}

	Bool Animation::HasTranslationChannel(Int nodeIndex)const
	{
		return translations_.contains(nodeIndex);
	}

	Int Animation::FindRootMotionNode()const
	{
		return translations_.empty() ? -1 : translations_.begin()->first;
	}

	void Animation::SamplePose(Float time, std::unordered_map<Int, Vector3>& outTranslations, std::unordered_map<Int, Quaternion>& outRotations, std::unordered_map<Int, Vector3>& outScales)const
	{
		auto findSegment = [](const DynamicArray<Float>& times, Float sampleTime, Float& outT) -> Size
		{
			if (times.size() <= 1)
			{
				outT = 0.0f;
				return 0;
			}

			if (sampleTime <= times.front())
			{
				outT = 0.0f;
				return 0;
			}

			if (sampleTime >= times.back())
			{
				outT = 0.0f;
				return times.size() - 1;
			}

			for (Size index = 0; index + 1 < times.size(); ++index)
			{
				if (sampleTime >= times[index] && sampleTime <= times[index + 1])
				{
					Float span = times[index + 1] - times[index];
					outT = span > 0.0f ? (sampleTime - times[index]) / span : 0.0f;
					return index;
				}
			}

			outT = 0.0f;
			return 0;
		};

		/// [EN] Only writes an entry for a channel that actually has keyframe
		///      data for that node — a node animating rotation only (the
		///      common case for skeletal joints) must NOT get a fabricated
		///      translation/scale override; the caller falls back to that
		///      node's own rest-pose value for whichever channels are absent.
		/// [JP] 実際にキーフレームデータがあるチャンネルのみ書き込む — 回転だけ
		///      アニメーションされるノード(スケルトンのジョイントで多いケース)に、
		///      でっち上げのtranslation/scaleを与えてはいけない。呼び出し側が、
		///      無いチャンネルはそのノード自身のレストポーズ値にフォールバックする。
		for (const auto& [nodeIndex, times] : timelines_)
		{
			auto translationIt = translations_.find(nodeIndex);
			if (translationIt != translations_.end())
			{
				const DynamicArray<Vector3>& values = translationIt->second;
				if (!values.empty())
				{
					if (values.size() == 1 || times.size() != values.size())
					{
						outTranslations[nodeIndex] = values[0];
					}
					else
					{
						Float t = 0.0f;
						Size index = findSegment(times, time, t);
						outTranslations[nodeIndex] = (index + 1 >= values.size()) ? values[index] : Vector3::Lerp(values[index], values[index + 1], t);
					}
				}
			}

			auto rotationIt = rotations_.find(nodeIndex);
			if (rotationIt != rotations_.end())
			{
				const DynamicArray<Quaternion>& values = rotationIt->second;
				if (!values.empty())
				{
					if (values.size() == 1 || times.size() != values.size())
					{
						outRotations[nodeIndex] = values[0];
					}
					else
					{
						Float t = 0.0f;
						Size index = findSegment(times, time, t);
						outRotations[nodeIndex] = (index + 1 >= values.size()) ? values[index] : Quaternion::Slerp(values[index], values[index + 1], t);
					}
				}
			}

			auto scaleIt = scales_.find(nodeIndex);
			if (scaleIt != scales_.end())
			{
				const DynamicArray<Vector3>& values = scaleIt->second;
				if (!values.empty())
				{
					if (values.size() == 1 || times.size() != values.size())
					{
						outScales[nodeIndex] = values[0];
					}
					else
					{
						Float t = 0.0f;
						Size index = findSegment(times, time, t);
						outScales[nodeIndex] = (index + 1 >= values.size()) ? values[index] : Vector3::Lerp(values[index], values[index + 1], t);
					}
				}
			}
		}
	}

	void Animation::SampleMorphWeights(Float time, std::unordered_map<Int, DynamicArray<Float>>& outWeights)const
	{
		auto findSegment = [](const DynamicArray<Float>& times, Float sampleTime, Float& outT) -> Size
		{
			if (times.size() <= 1)
			{
				outT = 0.0f;
				return 0;
			}

			if (sampleTime <= times.front())
			{
				outT = 0.0f;
				return 0;
			}

			if (sampleTime >= times.back())
			{
				outT = 0.0f;
				return times.size() - 1;
			}

			for (Size index = 0; index + 1 < times.size(); ++index)
			{
				if (sampleTime >= times[index] && sampleTime <= times[index + 1])
				{
					Float span = times[index + 1] - times[index];
					outT = span > 0.0f ? (sampleTime - times[index]) / span : 0.0f;
					return index;
				}
			}

			outT = 0.0f;
			return 0;
		};

		for (const auto& [nodeIndex, rows] : weights_)
		{
			if (rows.empty())
			{
				continue;
			}

			auto timelineIt = timelines_.find(nodeIndex);
			if (timelineIt == timelines_.end())
			{
				continue;
			}
			const DynamicArray<Float>& times = timelineIt->second;

			if (rows.size() == 1 || times.size() != rows.size())
			{
				outWeights[nodeIndex] = rows[0];
				continue;
			}

			Float t = 0.0f;
			Size index = findSegment(times, time, t);
			if (index + 1 >= rows.size())
			{
				outWeights[nodeIndex] = rows[index];
				continue;
			}

			const DynamicArray<Float>& from = rows[index];
			const DynamicArray<Float>& to = rows[index + 1];

			/// [EN] Rows can differ in width if a channel was ever authored
			///      against a different target count than the mesh has now
			///      — fall back to the earlier row's weight, unlerped, for
			///      any target index the later row doesn't cover.
			/// [JP] チャンネルが現在のメッシュのターゲット数と異なる本数で
			///      作成されていた場合、行ごとに幅が異なりうる — 後方の行が
			///      カバーしないターゲットインデックスは、前方の行の
			///      ウェイトをそのまま(補間せず)使う。
			DynamicArray<Float>& blended = outWeights[nodeIndex];
			blended.resize(from.size());
			for (Size targetIndex = 0; targetIndex < from.size(); ++targetIndex)
			{
				blended[targetIndex] = targetIndex < to.size() ? from[targetIndex] + (to[targetIndex] - from[targetIndex]) * t : from[targetIndex];
			}
		}
	}

	DynamicArray<AnimationSpeedKeyframe>& Animation::SpeedCurve()
	{
		return speedCurve_;
	}

	const DynamicArray<AnimationSpeedKeyframe>& Animation::SpeedCurve()const
	{
		return speedCurve_;
	}

	DynamicArray<AnimationNotifyEvent>& Animation::NotifyEvents()
	{
		return notifyEvents_;
	}

	const DynamicArray<AnimationNotifyEvent>& Animation::NotifyEvents()const
	{
		return notifyEvents_;
	}
}
