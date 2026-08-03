#pragma once
#include <FoundationEngine/Math/MathCommon.h>

namespace SeedCore
{
	/// [EN] 2D floating-point vector; alias for DirectX::SimpleMath::Vector2.
	/// [JP] 2次元浮動小数点ベクトル。DirectX::SimpleMath::Vector2 のエイリアス。
	using Vector2 = DirectX::SimpleMath::Vector2;

	/// [EN] 3D floating-point vector; alias for DirectX::SimpleMath::Vector3.
	/// [JP] 3次元浮動小数点ベクトル。DirectX::SimpleMath::Vector3 のエイリアス。
	using Vector3 = DirectX::SimpleMath::Vector3;

	/// [EN] 4D floating-point vector; alias for DirectX::SimpleMath::Vector4.
	/// [JP] 4次元浮動小数点ベクトル。DirectX::SimpleMath::Vector4 のエイリアス。
	using Vector4 = DirectX::SimpleMath::Vector4;

	/// [EN] RGBA color (stored as a 4D floating-point vector); alias for DirectX::SimpleMath::Color.
	/// [JP] RGBA カラー（4次元浮動小数点ベクトルとして格納される）。DirectX::SimpleMath::Color のエイリアス。
	using Color = DirectX::SimpleMath::Color;

	/// [EN] 2D unsigned integer vector; alias for DirectX::XMUINT2.
	/// [JP] 2次元符号なし整数ベクトル。DirectX::XMUINT2 のエイリアス。
	using XmUint2 = DirectX::XMUINT2;

	/// [EN] 3D unsigned integer vector; alias for DirectX::XMUINT3.
	/// [JP] 3次元符号なし整数ベクトル。DirectX::XMUINT3 のエイリアス。
	using XmUint3 = DirectX::XMUINT3;

	/// [EN] 4D unsigned integer vector; alias for DirectX::XMUINT4.
	/// [JP] 4次元符号なし整数ベクトル。DirectX::XMUINT4 のエイリアス。
	using XmUint4 = DirectX::XMUINT4;
}
