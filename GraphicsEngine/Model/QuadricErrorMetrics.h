#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Reference: https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf
	* 
	* Mesh simplification using Quadric Error Metrics (QEM).
	*
	* QEM is an algorithm that decides which edges in a mesh can be
	* collapsed (merged) with the least visible distortion. It works by
	* assigning each vertex a "quadric" — a compact representation of all
	* the planes (triangles) surrounding that vertex. When two vertices
	* are merged, their quadrics are simply added together, and the
	* resulting quadric tells us both WHERE to place the merged vertex
	* and HOW MUCH error the merge introduces.
	*
	* Overview of the pipeline:
	*
	*   1. For every triangle, compute its plane equation (normal + offset).
	*   2. Convert each plane into a 4x4 symmetric "quadric" matrix.
	*   3. For every vertex, sum the quadrics of all adjacent triangles.
	*   4. For every edge (v0, v1), combine the two endpoint quadrics,
	*      solve for the optimal merged position, and compute the cost.
	*   5. Put all edges into a min-heap sorted by cost.
	*   6. Repeatedly pop the cheapest edge, collapse it, and update
	*      neighbours until we reach the target triangle count.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 二次誤差メトリクス（QEM）によるメッシュ簡略化。
	*
	* QEM は、メッシュのどのエッジを崩壊（頂点を統合）すれば最も見た目の
	* 歪みが少ないかを判定するアルゴリズムである。各頂点にその周囲の
	* 平面（三角形）をコンパクトに表現した「二次形式行列（Quadric）」を
	* 割り当てる。2 つの頂点を統合する際は Quadric を単純に加算するだけで、
	* 統合後の最適な頂点位置と、統合により生じる誤差の両方が得られる。
	*
	* パイプラインの概要：
	*
	*   1. 各三角形について平面方程式（法線 + オフセット）を計算する。
	*   2. 各平面を 4x4 対称行列「Quadric」に変換する。
	*   3. 各頂点について、隣接する全三角形の Quadric を合算する。
	*   4. 各エッジ (v0, v1) について両端点の Quadric を合成し、
	*      最適な統合位置を求め、そのコストを計算する。
	*   5. 全エッジをコスト昇順のミニヒープに投入する。
	*   6. 最小コストのエッジを取り出して崩壊し、隣接エッジを更新する。
	*      目標三角形数に到達するまでこれを繰り返す。
	*/
	class QuadricErrorMetrics
	{
	public:
		/**
		* [EN]
		* The output of a simplification pass.
		*
		*   indices   — triangle index buffer after simplification
		*               (local indices into `positions`).
		*   vertexMap — for each surviving vertex i, vertexMap[i] is the
		*               index of the ORIGINAL vertex it came from.
		*               Use this to copy attributes (normal, texcoord, …)
		*               from the original vertex array.
		*   positions — vertex positions after simplification.
		*               May differ from the originals because QEM moves
		*               each merged vertex to its optimal position.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 簡略化パスの出力。
		*
		*   indices   — 簡略化後の三角形インデックスバッファ
		*               （`positions` へのローカルインデックス）。
		*   vertexMap — 生き残った各頂点 i について、vertexMap[i] は
		*               元の頂点配列における対応するインデックスを示す。
		*               法線・テクスチャ座標などの属性コピーに使用する。
		*   positions — 簡略化後の頂点座標。
		*               QEM は統合先の頂点を最適位置に移動するため、
		*               元の座標とは異なる場合がある。
		*/
		struct Result
		{
			DynamicArray<Uint32> indices;
			DynamicArray<Uint32> vertexMap;
			DynamicArray<Vector3> positions;
		};

		/**
		* [EN]
		* Simplifies a triangle mesh by repeatedly collapsing the
		* lowest-cost edge until the triangle count drops to
		* `targetTriangleCount` or no further collapse is possible.
		*
		* `locked` is an optional per-vertex lock flag array. A locked
		* vertex will not be moved (its position is preserved), but it
		* may absorb a neighbour. Pass nullptr to leave all vertices
		* unlocked.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最小コストのエッジを繰り返し崩壊させ、三角形数が
		* `targetTriangleCount` に達するか、これ以上崩壊できなくなるまで
		* 三角形メッシュを簡略化する。
		*
		* `locked` は頂点ごとのロックフラグ配列（任意）。ロックされた
		* 頂点は移動されない（位置が保持される）が、隣接頂点を吸収する
		* ことはできる。全頂点をロックしない場合は nullptr を渡す。
		*/
		Result Simplify(const Vector3* positions, Size vertexCount, const Uint32* indices, Size indexCount, Size targetTriangleCount, const Bool* locked = nullptr);

	private:
		/**
		* [EN]
		* A quadric is a 4×4 symmetric matrix that encodes the squared
		* distance from a point to a set of planes. Because the matrix is
		* symmetric, only 10 unique elements need to be stored:
		*
		*        | d[0]  d[1]  d[2]  d[3] |     | a²  ab  ac  aw |
		*   Q =  |       d[4]  d[5]  d[6] |  =  |     b²  bc  bw |
		*        |             d[7]  d[8] |     |         c²  cw |
		*        |                   d[9] |     |             w² |
		*
		* where (a, b, c) is the plane normal and
		* w = -(a·px + b·py + c·pz) is the plane offset (the signed
		* distance from the origin to the plane, negated).
		*
		* Key properties:
		*   - Adding two quadrics Q1 + Q2 gives the combined error of
		*     both plane sets. This is why vertex quadrics are computed
		*     as the SUM of all adjacent triangle quadrics.
		*   - Evaluating v^T · Q · v gives the total squared distance
		*     from point v to all the planes encoded in Q.
		*   - The point that minimises v^T · Q · v can be found by
		*     solving the upper-left 3×3 sub-system of Q.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Quadric は、ある点から平面群までの二乗距離の合計を符号化する
		* 4×4 対称行列である。対称行列なので、固有の要素は 10 個だけ
		* 格納すれば十分である:
		*
		*        | d[0]  d[1]  d[2]  d[3] |     | a²  ab  ac  aw |
		*   Q =  |       d[4]  d[5]  d[6] |  =  |     b²  bc  bw |
		*        |             d[7]  d[8] |     |         c²  cw |
		*        |                   d[9] |     |             w² |
		*
		* ここで (a, b, c) は平面の法線、
		* w = -(a·px + b·py + c·pz) は平面のオフセット
		* （原点から平面までの符号付き距離の符号反転）である。
		*
		* 主な性質:
		*   - 2 つの Quadric Q1 + Q2 を加算すると、両方の平面群に対する
		*     合成誤差が得られる。頂点の Quadric が隣接三角形の Quadric の
		*     「合計」として計算されるのはこの性質による。
		*   - v^T · Q · v を評価すると、Q に符号化された全平面から
		*     点 v までの二乗距離の合計が得られる。
		*   - v^T · Q · v を最小化する点は、Q の左上 3×3 部分行列から
		*     なる連立方程式を解くことで求められる。
		*/
		struct Quadric
		{
			/// [EN] The 10 unique elements of the 4×4 symmetric matrix. See diagram above.
			/// [JP] 4×4 対称行列の 10 個の固有要素。上図を参照。
			Double d[10] = {};

			/**
			* [EN]
			* Constructs a quadric from a single plane equation
			* ax + by + cz + w = 0. The resulting matrix is the outer
			* product of the plane vector with itself:
			*   Q = p · p^T  where p = (a, b, c, w)
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 単一の平面方程式 ax + by + cz + w = 0 から Quadric を構築する。
			* 得られる行列は平面ベクトルの外積（自身との直積）である:
			*   Q = p · p^T  ただし p = (a, b, c, w)
			*/
			void FromPlane(Double a, Double b, Double c, Double w);

			/**
			* [EN]
			* Adds another quadric to this one element-wise. This
			* corresponds to combining the error of two plane sets:
			* the error at any point v with respect to Q1 + Q2 equals
			* the sum of the errors with respect to Q1 and Q2 separately.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 別の Quadric を要素ごとに加算する。これは 2 つの平面群の
			* 誤差を合成することに相当する。任意の点 v に対して
			* Q1 + Q2 での誤差は、Q1 と Q2 それぞれでの誤差の合計に等しい。
			*/
			Quadric& operator+=(const Quadric& rhs);

			/**
			* [EN]
			* Computes v^T · Q · v, the total squared distance from the
			* point (x, y, z) to all planes encoded in this quadric.
			* This value is the "cost" of placing a vertex at (x, y, z).
			*
			* Expanded form:
			*   d[0]·x² + 2·d[1]·x·y  + 2·d[2]·x·z  + 2·d[3]·x
			*           + d[4]·y²     + 2·d[5]·y·z  + 2·d[6]·y
			*                         + d[7]·z²     + 2·d[8]·z
			*                                       + d[9]
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* v^T · Q · v を計算する。これは点 (x, y, z) からこの Quadric に
			* 符号化された全平面までの二乗距離の合計である。
			* この値が頂点を (x, y, z) に配置する「コスト」となる。
			*
			* 展開形:
			*   d[0]·x² + 2·d[1]·x·y  + 2·d[2]·x·z  + 2·d[3]·x
			*           + d[4]·y²     + 2·d[5]·y·z  + 2·d[6]·y
			*                         + d[7]·z²     + 2·d[8]·z
			*                                       + d[9]
			*/
			Double Evaluate(Double x, Double y, Double z)const;

			/**
			* [EN]
			* Finds the point (outX, outY, outZ) that minimises
			* v^T · Q · v by solving the 3×3 linear system:
			*
			*   | d[0]  d[1]  d[2] |   | x |     | -d[3] |
			*   | d[1]  d[4]  d[5] | · | y |  =  | -d[6] |
			*   | d[2]  d[5]  d[7] |   | z |     | -d[8] |
			*
			* This is the gradient of v^T·Q·v set to zero. The system
			* is solved using Cramer's rule (direct determinant formula).
			*
			* Returns false if the matrix is singular (determinant ≈ 0),
			* which happens when the surrounding planes are nearly
			* coplanar. In that case the caller should fall back to
			* using one of the original vertex positions instead.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* v^T · Q · v を最小化する点 (outX, outY, outZ) を、以下の
			* 3×3 連立方程式を解くことで求める:
			*
			*   | d[0]  d[1]  d[2] |   | x |     | -d[3] |
			*   | d[1]  d[4]  d[5] | · | y |  =  | -d[6] |
			*   | d[2]  d[5]  d[7] |   | z |     | -d[8] |
			*
			* これは v^T·Q·v の勾配をゼロに設定したものである。解法には
			* クラメルの公式（行列式の直接計算）を用いる。
			*
			* 行列が特異（行列式 ≈ 0）の場合は false を返す。これは周囲の
			* 平面がほぼ同一平面上にある場合に発生する。その場合、呼び出し側は
			* 元の頂点位置のいずれかにフォールバックする必要がある。
			*/
			Bool OptimalPosition(Double& outX, Double& outY, Double& outZ)const;
		};
	};
}