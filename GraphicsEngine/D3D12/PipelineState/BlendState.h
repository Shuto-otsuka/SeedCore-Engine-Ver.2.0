#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	enum class BlendStateType
	{
		Opaque,
		Alpha,

		/// [EN] Alpha blend for sources whose rgb is ALREADY multiplied by its
		///      own alpha (SrcBlend = ONE instead of SRC_ALPHA). Used by the OIT
		///      resolve pass, which accumulates premultiplied rgb internally -
		///      compositing that with the plain Alpha state would apply alpha a
		///      second time and darken every transparent surface.
		/// [JP] rgb が既に自身のアルファで乗算済みのソース向けアルファブレンド
		///      (SrcBlend が SRC_ALPHA ではなく ONE)。OIT リゾルブパスが使う -
		///      あちらは内部で事前乗算済み rgb を積算しているため、通常の Alpha
		///      で合成するとアルファが二重に掛かり、透明面が軒並み暗くなる。
		AlphaPremultiplied,

		Additive,
		Multiply,
		Subtractive,
		Screen,
	};

	class BlendState
	{
	public:
		static void Create();

		static D3D12_BLEND_DESC Get(BlendStateType type);
	};
}
