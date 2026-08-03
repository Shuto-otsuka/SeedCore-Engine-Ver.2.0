#pragma once

namespace SeedCore
{
	/**
	* [EN]
	* Xorshift random number generator implementation.
	*
	* Design characteristics:
	* - Provides extremely fast pseudo-random number generation
	* using only bitwise XOR and shift operations.
	* - Intended for performance-critical scenarios such as physics
	* simulations, particle effects, and gameplay variance.
	*
	* Important:
	* - Not cryptographically secure; do not use for sensitive data.
	* - If the state is set to 0, the sequence will lock at 0;
	* implement guards if necessary.
	*
	* TODO:
	* - Consider migrating to newer PRNGs like xoshiro256** for better distribution.
	* - Add helper methods for mapped ranges (e.g., [0.0, 1.0) float/double).
	* - Investigate thread-local storage patterns for lock-free parallel access.
	*
	* [JP]
	* Xorshift 乱数生成器の実装。
	*
	* 設計思想:
	* - ビットシフトとXOR演算のみで構成される極めて高速な疑似乱数生成アルゴリズム。
	* - 物理シミュレーションやゲームエンジンにおけるパーティクル生成など、
	* 大量かつ高速な乱数生成が要求される箇所での利用を想定。
	*
	* 注意:
	* - 暗号学的な安全性はないため、セキュリティ用途には使用不可。
	* - シード値に0を設定すると、以降の計算結果が0に固定されるため注意が必要。
	*
	* TODO:
	* - xoshiro256** 等のより近代的なアルゴリズムへの換装検討。
	* - 乱数の範囲指定（[0, 1) の float/double 等）を行うヘルパー関数の追加。
	* - スレッドローカルなインスタンス管理パターンの検討。
	*/
	template<typename T>
	class Xorshift
	{
	public:
		/**
		* [EN]
		* Default constructor: leaves state_ default-initialized (must be
		* seeded via Seed before use, since a zero state locks the sequence at 0).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: state_ をデフォルト初期化のままにする
		* （状態が 0 だと系列が 0 に固定されるため、使用前に Seed による
		* シード設定が必要）。
		*/
		Xorshift() = default;

		/**
		* [EN]
		* Constructs, seeding the generator with value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value でジェネレータをシード設定して構築する。
		*/
		Xorshift(T value) :state_(value)
		{
			/// No Code
		}

		/**
		* [EN]
		* Re-seeds the generator's internal state with value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ジェネレータの内部状態を value で再シード設定する。
		*/
		void Seed(T value)
		{
			state_ = value;
		}

		/**
		* [EN]
		* Advances the internal state via the xorshift bit-mixing steps
		* (specialized for 32-bit and 64-bit T) and returns the next
		* pseudo-random value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* xorshift のビット混合ステップ（32ビット/64ビットの T に対して
		* それぞれ特殊化）で内部状態を進め、次の疑似乱数値を返す。
		*/
		T operator()()
		{
			if constexpr (sizeof(T) == 8)
			{
				/// [EN] 64-bit xorshift: three shift-XOR rounds (13/-7/17), then a final multiply (xorshift*) to improve output distribution.
				/// [JP] 64ビット xorshift: 3回のシフトXORラウンド（13/-7/17）の後、出力分布を改善するための最終乗算（xorshift*）を行う。
				state_ ^= state_ << 13;
				state_ ^= state_ >> 7;
				state_ ^= state_ << 17;
				return state_ * 0x2545F4914F6CDD1DULL;
			}
			else if constexpr (sizeof(T) == 4)
			{
				/// [EN] 32-bit xorshift: the classic three shift-XOR rounds (13/-7/5), returned directly (no multiply step).
				/// [JP] 32ビット xorshift: 古典的な3回のシフトXORラウンド（13/-7/5）を行い、そのまま返す（乗算ステップなし）。
				state_ ^= state_ << 13;
				state_ ^= state_ >> 7;
				state_ ^= state_ << 5;
				return state_;
			}
			else
			{
				/// [EN] Unsupported width: no bit-mixing constants are defined for sizes other than 32/64 bits.
				/// [JP] 未対応の幅: 32/64ビット以外のサイズに対するビット混合定数は定義されていない。
				// TODO
			}
		}

	private:
		/// [EN] The generator's internal state, updated in place by each operator() call.
		/// [JP] ジェネレータの内部状態。operator() の呼び出しごとにその場で更新される。
		T state_;
	};
}