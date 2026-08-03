#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] Satisfied when T is safe to store as ECS component data: trivially copyable and standard-layout, so it can be freely memcpy'd/relocated within archetype chunk storage.
	/// [JP] T が ECS のコンポーネントデータとして格納しても安全である場合に満たされる。トリビアルにコピー可能かつ standard-layout であり、アーキタイプのチャンクストレージ内で自由に memcpy/再配置できることを保証する。
	template<typename T>
	concept IsComponent = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;
}
