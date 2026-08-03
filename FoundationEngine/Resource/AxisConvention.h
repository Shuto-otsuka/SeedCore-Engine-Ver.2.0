#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* One of the 6 signed cardinal directions selectable for an Up/Right/
	* Forward axis picker.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Up/Right/Forward軸ピッカーで選択できる、符号付きの6つの基本方向のいずれか。
	*/
	enum class SignedAxis :Uint32
	{
		PositiveX = 0,
		NegativeX = 1,
		PositiveY = 2,
		NegativeY = 3,
		PositiveZ = 4,
		NegativeZ = 5,
	};

	/**
	* [EN]
	* Manual override for triangle winding order, applied independently
	* of the axis-derived handedness. Auto never touches index order
	* (matches the engine's existing single-axis-mirror RH->LH behavior,
	* where the winding flip happens implicitly through vertex mirroring
	* alone); AsIs/Flip let the user force index order explicitly for
	* edge cases Auto doesn't handle correctly.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 三角形の巻き順に対する手動オーバーライド。軸から導出される利き手とは
	* 独立に適用する。Autoはインデックス順を一切変更しない（エンジン既存の
	* 単一軸ミラーによるRH->LH変換と同じ挙動 — 巻き順の反転は頂点ミラーの
	* 副作用として暗黙的に起きる）。AsIs/FlipはAutoで正しく扱えない
	* エッジケース向けに、インデックス順を明示的に強制する。
	*/
	enum class WindingOverride :Uint32
	{
		Auto = 0,
		AsIs = 1,
		Flip = 2,
	};

	/**
	* [EN]
	* Describes a source coordinate convention as three independently
	* picked signed axes (Up/Right/Forward), plus a manual winding
	* override. Handedness is intentionally not stored here - it is
	* always derived from the three picks by ResolvedAxisConvention::Resolve.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ソースの座標系規約を、独立に選択された3つの符号付き軸(Up/Right/
	* Forward)と手動の巻き順オーバーライドで表す。利き手は意図的にここへ
	* 保存しない — 常に ResolvedAxisConvention::Resolve が3つの選択から導出する。
	*/
	struct SEEDCORE_API AxisConvention
	{
		SignedAxis up_ = SignedAxis::PositiveY;
		SignedAxis right_ = SignedAxis::NegativeX;
		SignedAxis forward_ = SignedAxis::PositiveZ;
		WindingOverride windingOverride_ = WindingOverride::Auto;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("up", up_),
				cereal::make_nvp("right", right_),
				cereal::make_nvp("forward", forward_),
				cereal::make_nvp("winding_override", windingOverride_)
			);
		}
	};

	/**
	* [EN]
	* Result of resolving an AxisConvention into a usable transform: the
	* 3x3 basis (as a signed-permutation Matrix; translation row is
	* identity), whether the three picks form a valid orthogonal basis,
	* the derived handedness/mirror state, and the winding-flip decision
	* after folding in windingOverride_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* AxisConvention を使用可能な変換へ解決した結果: 3x3基底（符号付き
	* 置換 Matrix、平行移動行は単位）、3つの選択が有効な直交基底を成すか、
	* 導出された利き手/ミラー状態、windingOverride_ を織り込んだ後の
	* 巻き順反転の要否。
	*/
	struct SEEDCORE_API ResolvedAxisConvention
	{
		/// [EN] Signed-permutation, orthonormal basis matrix (v' = Vector3::Transform(v, basis_)).
		/// [JP] 符号付き置換の正規直交基底行列 (v' = Vector3::Transform(v, basis_))。
		Matrix basis_ = Matrix::Identity;

		/// [EN] False when two of the three picks name the same underlying axis.
		/// [JP] 3つの選択のうち2つが同じ軸を指している場合はfalse。
		Bool valid_ = false;

		/// [EN] True when the basis is a mirror relative to the engine's native LH space (det(basis_) < 0).
		/// [JP] 基底がエンジン標準のLH空間に対してミラーになっている場合true (det(basis_) < 0)。
		Bool isMirror_ = false;

		/// [EN] True when this convention describes a right-handed source (equivalent to isMirror_).
		/// [JP] このコンベンションが右手系のソースを表す場合true (isMirror_と等価)。
		Bool isRightHanded_ = false;

		/// [EN] Whether triangle index order should be swapped. Auto always resolves to false.
		/// [JP] 三角形のインデックス順を入れ替えるべきか。Autoは常にfalseに解決する。
		Bool flipWinding_ = false;

		[[nodiscard]] static ResolvedAxisConvention Resolve(const AxisConvention& convention);
	};
}
