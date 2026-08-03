#include <GraphicsEngine/Model/Animation/Animation.h>

namespace SeedCore
{
	Float Animation::Duration()const
	{
		return duration_;
	}

	Bool Animation::HasTranslationChannel(Int nodeIndex)const
	{
		return translations_.find(nodeIndex) != translations_.end();
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
