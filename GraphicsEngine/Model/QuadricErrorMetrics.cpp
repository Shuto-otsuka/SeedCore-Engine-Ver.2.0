#include <GraphicsEngine/Model/QuadricErrorMetrics.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a quadric matrix from a plane equation ax + by + cz + w = 0.
	*
	* A plane in 3D space can be written as a 4-component vector p = (a, b, c, w).
	* The "quadric" for this plane is the 4×4 matrix Q = p · p^T (outer product).
	*
	* Conceptually, this matrix encodes "how far is a point from this plane,
	* squared?" If you evaluate v^T · Q · v for any point v = (x, y, z, 1),
	* you get (ax + by + cz + w)², which is the squared distance from v to the
	* plane (assuming (a, b, c) is unit-length).
	*
	* The outer product p · p^T expands to:
	*
	*   | a |                               | a·a  a·b  a·c  a·w |
	*   | b | × [ a  b  c  w ]  =          | b·a  b·b  b·c  b·w |
	*   | c |                               | c·a  c·b  c·c  c·w |
	*   | w |                               | w·a  w·b  w·c  w·w |
	*
	* Because p·p^T is symmetric (row i, col j == row j, col i), only 10 of
	* the 16 elements are unique. We store them in d[0..9]:
	*
	*   d[0]=a·a  d[1]=a·b  d[2]=a·c  d[3]=a·w
	*             d[4]=b·b  d[5]=b·c  d[6]=b·w
	*                       d[7]=c·c  d[8]=c·w
	*                                 d[9]=w·w
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 平面方程式 ax + by + cz + w = 0 から Quadric 行列を構築する。
	*
	* 3D 空間の平面は 4 成分ベクトル p = (a, b, c, w) で表せる。
	* この平面の「Quadric」は 4×4 行列 Q = p · p^T（外積）である。
	*
	* この行列は「ある点がこの平面からどれだけ離れているか（二乗）」を符号化する。
	* 任意の点 v = (x, y, z, 1) について v^T · Q · v を評価すると、
	* (ax + by + cz + w)² が得られる。これは v から平面までの二乗距離である
	* （(a, b, c) が単位長であると仮定）。
	*
	* 外積 p · p^T を展開すると:
	*
	*   | a |                               | a·a  a·b  a·c  a·w |
	*   | b | × [ a  b  c  w ]  =          | b·a  b·b  b·c  b·w |
	*   | c |                               | c·a  c·b  c·c  c·w |
	*   | w |                               | w·a  w·b  w·c  w·w |
	*
	* p·p^T は対称行列なので（行 i 列 j == 行 j 列 i）、16 要素のうち
	* 固有なのは 10 個だけである。それを d[0..9] に格納する:
	*
	*   d[0]=a·a  d[1]=a·b  d[2]=a·c  d[3]=a·w
	*             d[4]=b·b  d[5]=b·c  d[6]=b·w
	*                       d[7]=c·c  d[8]=c·w
	*                                 d[9]=w·w
	*/
	void QuadricErrorMetrics::Quadric::FromPlane(Double a, Double b, Double c, Double w)
	{
		d[0] = a * a;
		d[1] = a * b;
		d[2] = a * c;
		d[3] = a * w;
		d[4] = b * b;
		d[5] = b * c;
		d[6] = b * w;
		d[7] = c * c;
		d[8] = c * w;
		d[9] = w * w;
	}

	/**
	* [EN]
	* Adds another quadric element-wise: this->d[i] += rhs.d[i].
	*
	* Why does simple addition work? Because the quadric error for a
	* VERTEX is defined as the sum of errors to all surrounding planes:
	*
	*   Q_vertex = Q_plane0 + Q_plane1 + Q_plane2 + ...
	*
	* And when we COLLAPSE an edge (merge vertex A and vertex B into one),
	* the merged vertex inherits ALL the planes from both A and B:
	*
	*   Q_merged = Q_A + Q_B
	*
	* This is the key insight of the QEM paper: a single 10-element
	* array carries all the geometric information needed to compute the
	* error at any point, and combining two sets of planes is just
	* element-wise addition. No need to track individual planes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 別の Quadric を要素ごとに加算する: this->d[i] += rhs.d[i]。
	*
	* なぜ単純な加算で良いのか？ 
	* 頂点の Quadric 誤差は、周囲の全平面への誤差の合計として定義されるからである:
	*
	*   Q_vertex = Q_plane0 + Q_plane1 + Q_plane2 + ...
	*
	* エッジを崩壊（頂点 A と頂点 B を 1 つに統合）する場合、統合後の
	* 頂点は A と B の両方の平面をすべて引き継ぐ:
	*
	*   Q_merged = Q_A + Q_B
	*
	* これが QEM 論文の核心的な洞察である。10 要素の配列 1 つに、任意の点での
	* 誤差を計算するために必要な幾何学的情報がすべて含まれており、
	* 2 つの平面群の合成は要素ごとの加算で済む。個別の平面を追跡する必要はない。
	*/
	QuadricErrorMetrics::Quadric& QuadricErrorMetrics::Quadric::operator+=(const Quadric& rhs)
	{
		for (Int index = 0; index < 10; index++)
		{
			d[index] += rhs.d[index];
		}
		return *this;
	}

	/**
	* [EN]
	* Evaluates v^T · Q · v for the point (x, y, z).
	*
	* This computes the total squared distance from the point to all
	* planes encoded in this quadric. The formula expands the matrix
	* multiplication v^T · Q · v where v = (x, y, z, 1):
	*
	*                     | d[0]  d[1]  d[2]  d[3] |   | x |
	*   [x  y  z  1]  ·   | d[1]  d[4]  d[5]  d[6] | · | y |
	*                     | d[2]  d[5]  d[7]  d[8] |   | z |
	*                     | d[3]  d[6]  d[8]  d[9] |   | 1 |
	*
	* Expanding row-by-row and collecting terms:
	*
	*   = d[0]·x² + 2·d[1]·x·y  + 2·d[2]·x·z  + 2·d[3]·x
	*             + d[4]·y²     + 2·d[5]·y·z  + 2·d[6]·y
	*                           + d[7]·z²     + 2·d[8]·z
	*                                         + d[9]
	*
	* The factor of 2 on the off-diagonal terms comes from the symmetry
	* of the matrix: d[1] appears once as row 0 col 1, and once as
	* row 1 col 0, so x·y is multiplied by d[1] twice.
	*
	* This value is always >= 0 for a valid quadric (since it is a sum
	* of squared distances). We use this as the "cost" of placing a
	* vertex at (x, y, z) — lower cost means less distortion.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 点 (x, y, z) に対して v^T · Q · v を評価する。
	*
	* この Quadric に符号化された全平面からその点までの二乗距離の合計を計算する。
	* v = (x, y, z, 1) として行列積 v^T · Q · v を展開した式である:
	*
	*                     | d[0]  d[1]  d[2]  d[3] |   | x |
	*   [x  y  z  1]  ·   | d[1]  d[4]  d[5]  d[6] | · | y |
	*                     | d[2]  d[5]  d[7]  d[8] |   | z |
	*                     | d[3]  d[6]  d[8]  d[9] |   | 1 |
	*
	* 行ごとに展開して項を整理すると:
	*
	*   = d[0]·x² + 2·d[1]·x·y  + 2·d[2]·x·z  + 2·d[3]·x
	*             + d[4]·y²     + 2·d[5]·y·z  + 2·d[6]·y
	*                           + d[7]·z²     + 2·d[8]·z
	*                                         + d[9]
	*
	* 非対角要素に 2 がつくのは、行列が対称であるため。d[1] は行 0 列 1 と
	* 行 1 列 0 の両方に現れるので、x·y には d[1] が 2 回掛かる。
	*
	* この値は有効な Quadric では常に >= 0 である（二乗距離の合計であるため）。
	* これを頂点を (x, y, z) に配置する「コスト」として使う。
	* コストが低いほど歪みが少ない。
	*/
	Double QuadricErrorMetrics::Quadric::Evaluate(Double x, Double y, Double z)const
	{
		return d[0] * x * x + 2.0 * d[1] * x * y + 2.0 * d[2] * x * z + 2.0 * d[3] * x
			+ d[4] * y * y + 2.0 * d[5] * y * z + 2.0 * d[6] * y
			+ d[7] * z * z + 2.0 * d[8] * z
			+ d[9];
	}

	/**
	* [EN]
	* Finds the point (outX, outY, outZ) that minimises v^T · Q · v.
	*
	* To minimise a quadratic function, we take its gradient (partial
	* derivatives) and set them to zero. For v^T · Q · v with
	* v = (x, y, z, 1), the gradient with respect to (x, y, z) gives
	* the 3×3 linear system:
	*
	*   | d[0]  d[1]  d[2] |   | x |     | -d[3] |
	*   | d[1]  d[4]  d[5] | · | y |  =  | -d[6] |
	*   | d[2]  d[5]  d[7] |   | z |     | -d[8] |
	*
	* We solve this using Cramer's rule. Cramer's rule says:
	*
	*   x = det(Ax) / det(A)
	*   y = det(Ay) / det(A)
	*   z = det(Az) / det(A)
	*
	* where Ax is the matrix A with its first column replaced by the
	* right-hand side vector, Ay with the second column replaced, etc.
	*
	* For a 3×3 matrix, det(A) can be expanded as:
	*
	*   det = a·(e·h - f·f) - b·(b·h - f·c) + c·(b·f - e·c)
	*
	* where a=d[0], b=d[1], c=d[2], e=d[4], f=d[5], h=d[7].
	*
	* If det ≈ 0, the matrix is singular — the planes are nearly
	* coplanar (all facing roughly the same direction), so there is no
	* single optimal point. In that case we return false and let the
	* caller fall back to one of the original vertex positions.
	*
	* Instead of computing det(Ax), det(Ay), det(Az) directly, we
	* compute the cofactor matrix (adjugate) and multiply by the
	* right-hand side. The cofactors of a symmetric 3×3 matrix are:
	*
	*   cofactor[0][0] = e·h - f·f       (for x, with rightX)
	*   cofactor[0][1] = f·c - b·h       (for x, with rightY)
	*   cofactor[0][2] = b·f - e·c       (for x, with rightZ)
	*   cofactor[1][0] = f·c - b·h       (for y, with rightX)
	*   cofactor[1][1] = a·h - c·c       (for y, with rightY)
	*   cofactor[1][2] = b·c - a·f       (for y, with rightZ)
	*   cofactor[2][0] = b·f - e·c       (for z, with rightX)
	*   cofactor[2][1] = b·c - a·f       (for z, with rightY)
	*   cofactor[2][2] = a·e - b·b       (for z, with rightZ)
	*
	* Then: x = (1/det) · (cof[0][0]·rightX + cof[0][1]·rightY + cof[0][2]·rightZ)
	*       y = (1/det) · (cof[1][0]·rightX + cof[1][1]·rightY + cof[1][2]·rightZ)
	*       z = (1/det) · (cof[2][0]·rightX + cof[2][1]·rightY + cof[2][2]·rightZ)
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* v^T · Q · v を最小化する点 (outX, outY, outZ) を求める。
	*
	* 二次関数を最小化するには、勾配（偏微分）をゼロに設定する。
	* v = (x, y, z, 1) としたとき、v^T · Q · v の (x, y, z) に関する
	* 勾配は以下の 3×3 連立方程式を与える:
	*
	*   | d[0]  d[1]  d[2] |   | x |     | -d[3] |
	*   | d[1]  d[4]  d[5] | · | y |  =  | -d[6] |
	*   | d[2]  d[5]  d[7] |   | z |     | -d[8] |
	*
	* これをクラメルの公式で解く。クラメルの公式は:
	*
	*   x = det(Ax) / det(A)
	*   y = det(Ay) / det(A)
	*   z = det(Az) / det(A)
	*
	* ここで Ax は行列 A の第 1 列を右辺ベクトルで置き換えたもの、
	* Ay は第 2 列、Az は第 3 列を置き換えたものである。
	*
	* 3×3 行列の行列式は以下のように展開できる:
	*
	*   det = a·(e·h - f·f) - b·(b·h - f·c) + c·(b·f - e·c)
	*
	* ここで a=d[0], b=d[1], c=d[2], e=d[4], f=d[5], h=d[7]。
	*
	* det ≈ 0 の場合、行列は特異である。つまり平面がほぼ同一平面上にあり
	* （すべてほぼ同じ方向を向いている）、唯一の最適点が存在しない。
	* その場合は false を返し、呼び出し側で元の頂点位置にフォールバックさせる。
	*
	* det(Ax), det(Ay), det(Az) を直接計算する代わりに、余因子行列
	* （随伴行列）を計算して右辺ベクトルと掛ける。対称 3×3 行列の余因子は:
	*
	*   cofactor[0][0] = e·h - f·f       （x 用、rightX と掛ける）
	*   cofactor[0][1] = f·c - b·h       （x 用、rightY と掛ける）
	*   cofactor[0][2] = b·f - e·c       （x 用、rightZ と掛ける）
	*   cofactor[1][0] = f·c - b·h       （y 用、rightX と掛ける）
	*   cofactor[1][1] = a·h - c·c       （y 用、rightY と掛ける）
	*   cofactor[1][2] = b·c - a·f       （y 用、rightZ と掛ける）
	*   cofactor[2][0] = b·f - e·c       （z 用、rightX と掛ける）
	*   cofactor[2][1] = b·c - a·f       （z 用、rightY と掛ける）
	*   cofactor[2][2] = a·e - b·b       （z 用、rightZ と掛ける）
	*
	* そして: x = (1/det) · (cof[0][0]·rightX + cof[0][1]·rightY + cof[0][2]·rightZ)
	*         y = (1/det) · (cof[1][0]·rightX + cof[1][1]·rightY + cof[1][2]·rightZ)
	*         z = (1/det) · (cof[2][0]·rightX + cof[2][1]·rightY + cof[2][2]·rightZ)
	*/
	Bool QuadricErrorMetrics::Quadric::OptimalPosition(Double& outX, Double& outY, Double& outZ)const
	{
		/// [EN] Extract the 6 unique elements of the upper-left 3×3 sub-matrix.
		/// [JP] 左上 3×3 部分行列の 6 つの固有要素を取り出す。
		Double a = d[0];
		Double b = d[1];
		Double c = d[2];
		Double e = d[4];
		Double f = d[5];
		Double h = d[7];

		/// [EN] Compute the determinant of the 3×3 coefficient matrix using cofactor expansion along the first row.
		/// [JP] 第 1 行に沿った余因子展開で 3×3 係数行列の行列式を計算する。
		Double determinant = a * (e * h - f * f) - b * (b * h - f * c) + c * (b * f - e * c);

		/// [EN] If the determinant is near zero, the matrix is singular and no unique solution exists.
		/// [JP] 行列式がほぼゼロなら行列は特異であり、一意な解は存在しない。
		if (std::abs(determinant) < 1e-15)
		{
			return false;
		}

		Double inverseDeterminant = 1.0 / determinant;

		/// [EN] The right-hand side of the system: -d[3], -d[6], -d[8] (negated plane offsets).
		/// [JP] 連立方程式の右辺: -d[3], -d[6], -d[8]（平面オフセットの符号反転）。
		Double rightX = -d[3];
		Double rightY = -d[6];
		Double rightZ = -d[8];

		/// [EN] Multiply the cofactor (adjugate) matrix by the right-hand side, scaled by 1/det.
		/// [JP] 余因子（随伴）行列と右辺ベクトルの積を 1/det でスケーリングする。
		outX = inverseDeterminant * ((e * h - f * f) * rightX + (f * c - b * h) * rightY + (b * f - e * c) * rightZ);
		outY = inverseDeterminant * ((f * c - b * h) * rightX + (a * h - c * c) * rightY + (b * c - a * f) * rightZ);
		outZ = inverseDeterminant * ((b * f - e * c) * rightX + (b * c - a * f) * rightY + (a * e - b * b) * rightZ);

		return true;
	}

	/**
	* [EN]
	* The main simplification loop. High-level overview:
	*
	*   ┌──────────────────────────────────────────────────────────────┐
	*   │  PHASE 1: Build per-vertex quadrics                          │
	*   │                                                              │
	*   │  For each triangle:                                          │
	*   │    1. Compute the plane equation (normal + offset)           │
	*   │    2. Build a Quadric from that plane                        │
	*   │    3. Add it to all 3 vertices of the triangle               │
	*   │                                                              │
	*   │  After this, each vertex's quadric encodes the total         │
	*   │  squared error to all of its surrounding triangles.          │
	*   ├──────────────────────────────────────────────────────────────┤
	*   │  PHASE 2: Build the edge priority queue                      │
	*   │                                                              │
	*   │  For each unique edge (v0, v1):                              │
	*   │    1. Combine Q_v0 + Q_v1                                    │
	*   │    2. Find the optimal merged position                       │
	*   │    3. Compute the cost = error at that position              │
	*   │    4. Push into a min-heap sorted by cost                    │
	*   ├──────────────────────────────────────────────────────────────┤
	*   │  PHASE 3: Greedy edge collapse                               │
	*   │                                                              │
	*   │  While triangleCount > target AND heap is not empty:         │
	*   │    1. Pop the cheapest edge                                  │
	*   │    2. Skip if stale (version mismatch) or endpoints dead     │
	*   │    3. Collapse: merge killVertex INTO keepVertex             │
	*   │       - Move keepVertex to optimal position                  │
	*   │       - Add killVertex's quadric to keepVertex's             │
	*   │       - Rewrite all triangles that used killVertex           │
	*   │       - Kill degenerate triangles (2 identical vertices)     │
	*   │       - Re-cost all edges touching keepVertex                │
	*   ├──────────────────────────────────────────────────────────────┤
	*   │  PHASE 4: Compact output                                     │
	*   │                                                              │
	*   │  Collect surviving vertices and triangles into a dense       │
	*   │  output buffer. Build vertexMap to trace back to originals.  │
	*   └──────────────────────────────────────────────────────────────┘
	*
	* Edge collapse visualised:
	*
	*   BEFORE collapse of edge (v1, v2):
	*
	*        v0                  v0
	*       / \                 / \
	*      /   \               /   \
	*    v3 --- v1 --- v2 --- v4    These two triangles (v3-v1-v2)
	*      \   /       \   /        and (v1-v2-v4) are "degenerate"
	*       \ /         \ /         after collapse and will be killed.
	*        v5          v6
	*
	*   AFTER collapse (v2 is killed, merged into v1):
	*
	*        v0
	*       / \
	*      /   \
	*    v3 --- v1* --- v4     v1* is moved to optimal position.
	*      \   /   \   /       All references to v2 become v1.
	*       \ /     \ /        The two degenerate triangles are gone.
	*        v5      v6
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メインの簡略化ループ。全体の概要:
	*
	*   ┌──────────────────────────────────────────────────────────────┐
	*   │  フェーズ 1: 頂点ごとの Quadric を構築                         │
	*   │                                                                │
	*   │  各三角形について:                                             │
	*   │    1. 平面方程式（法線 + オフセット）を計算                    │
	*   │    2. その平面から Quadric を構築                              │
	*   │    3. 三角形の 3 頂点すべてに加算                              │
	*   │                                                                │
	*   │  この後、各頂点の Quadric は周囲の全三角形への                 │
	*   │  二乗誤差の合計を符号化している。                              │
	*   ├──────────────────────────────────────────────────────────────┤
	*   │  フェーズ 2: エッジ優先度キューの構築                          │
	*   │                                                                │
	*   │  各一意なエッジ (v0, v1) について:                             │
	*   │    1. Q_v0 + Q_v1 を合成                                       │
	*   │    2. 最適な統合位置を求める                                   │
	*   │    3. コスト = その位置での誤差を計算                          │
	*   │    4. コスト昇順のミニヒープに投入                             │
	*   ├──────────────────────────────────────────────────────────────┤
	*   │  フェーズ 3: 貪欲なエッジ崩壊                                  │
	*   │                                                                │
	*   │  三角形数 > 目標 かつ ヒープが空でない間:                      │
	*   │    1. 最小コストのエッジを取り出す                             │
	*   │    2. 古い場合（バージョン不一致）や端点が死んでいたらスキップ │
	*   │    3. 崩壊: killVertex を keepVertex に統合                    │
	*   │       - keepVertex を最適位置に移動                            │
	*   │       - killVertex の Quadric を keepVertex に加算             │
	*   │       - killVertex を使う全三角形を書き換え                    │
	*   │       - 退化三角形（同一頂点 2 つ）を削除                      │
	*   │       - keepVertex に接する全エッジのコストを再計算            │
	*   ├──────────────────────────────────────────────────────────────┤
	*   │  フェーズ 4: 出力のコンパクト化                                │
	*   │                                                                │
	*   │  生き残った頂点と三角形を密なバッファに収集する。              │
	*   │  元の頂点への対応を示す vertexMap を構築する。                 │
	*   └──────────────────────────────────────────────────────────────┘
	*
	* エッジ崩壊の図解:
	*
	*   エッジ (v1, v2) の崩壊前:
	*
	*        v0                  v0
	*       / \                 / \
	*      /   \               /   \
	*    v3 --- v1 --- v2 --- v4    この 2 つの三角形 (v3-v1-v2) と
	*      \   /       \   /        (v1-v2-v4) は崩壊後に「退化」し
	*       \ /         \ /         削除される。
	*        v5          v6
	*
	*   崩壊後（v2 は削除され、v1 に統合）:
	*
	*        v0
	*       / \
	*      /   \
	*    v3 --- v1* --- v4     v1* は最適位置に移動済み。
	*      \   /   \   /       v2 への参照はすべて v1 になる。
	*       \ /     \ /        退化した 2 つの三角形は消滅。
	*        v5      v6
	*/
	QuadricErrorMetrics::Result QuadricErrorMetrics::Simplify(const Vector3* sourcePositions, Size vertexCount, const Uint32* sourceIndices, Size indexCount, Size targetTriangleCount, const Bool* locked)
	{
		Size triangleCount = indexCount / 3;

		/// [EN] Early out: if the mesh already has fewer triangles than the target, return it unchanged.
		/// [JP] 早期リターン: メッシュの三角形数が既に目標以下なら、変更せずそのまま返す。
		if (targetTriangleCount >= triangleCount)
		{
			Result result;
			result.positions.assign(sourcePositions, sourcePositions + vertexCount);
			result.indices.assign(sourceIndices, sourceIndices + indexCount);
			result.vertexMap.resize(vertexCount);
			for (Size index = 0; index < vertexCount; index++)
			{
				result.vertexMap[index] = static_cast<Uint32>(index);
			}
			return result;
		}

		/// [EN] Working copies — we modify positions and indices in-place
		///      during collapse, so we copy the originals first.
		/// [JP] 作業用コピー — 崩壊中に位置とインデックスを直接書き換えるため、
		///      元のデータを先にコピーする。
		DynamicArray<Vector3> positions(sourcePositions, sourcePositions + vertexCount);
		DynamicArray<Uint32> indices(sourceIndices, sourceIndices + indexCount);

		/// [EN] Per-vertex quadric — starts as zero, accumulates from adjacent triangles.
		/// [JP] 頂点ごとの Quadric — ゼロ初期化、隣接三角形から累積する。
		DynamicArray<Quadric> quadrics(vertexCount);

		/// [EN] Per-vertex list of triangle indices that reference this vertex.
		///      Used to quickly find which triangles need updating when a vertex is killed.
		/// [JP] 各頂点を参照する三角形インデックスのリスト。
		///      頂点が削除されたとき、どの三角形を更新すべきかを高速に見つけるために使う。
		DynamicArray<DynamicArray<Uint32>> vertexTriangles(vertexCount);

		/// [EN] Tracks which triangles / vertices have been removed by edge collapses.
		/// [JP] エッジ崩壊によって除去された三角形・頂点を追跡するフラグ。
		DynamicArray<Bool> triangleDead(triangleCount, false);
		DynamicArray<Bool> vertexDead(vertexCount, false);

		/// [EN] PHASE 1: Build per-vertex quadrics from all triangles.
		/// [JP] フェーズ 1: 全三角形から頂点ごとの Quadric を構築する。
		Size degenerateTriangleCount = 0;
		for (Size triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
		{
			Uint32 index0 = indices[triangleIndex * 3];
			Uint32 index1 = indices[triangleIndex * 3 + 1];
			Uint32 index2 = indices[triangleIndex * 3 + 2];

			/// [EN] Compute two edge vectors of the triangle.
			///      edge1 = v1 - v0, edge2 = v2 - v0.
			/// [JP] 三角形の 2 つの辺ベクトルを計算する。
			///      edge1 = v1 - v0, edge2 = v2 - v0。
			Vector3 edge1 = positions[index1] - positions[index0];
			Vector3 edge2 = positions[index2] - positions[index0];

			/// [EN] Cross product gives the triangle's normal (unnormalised).
			///      If its length is near zero, the triangle is degenerate (zero area).
			///      Mark it dead so edge collapses don't leave stale indices in the output.
			/// [JP] 外積で三角形の法線を得る（未正規化）。
			///      長さがほぼゼロなら退化三角形（面積ゼロ）。
			///      エッジ崩壊が出力に古いインデックスを残さないよう dead にする。
			Vector3 normal = edge1.Cross(edge2);
			Float length = normal.Length();
			if (length < 1e-8f)
			{
				triangleDead[triangleIndex] = true;
				degenerateTriangleCount++;
				continue;
			}
			normal /= length;

			/// [EN] Build the plane equation: normal · point + w = 0.
			///      w = -(normal · v0), so that v0 lies exactly on the plane.
			///      We promote to Double for precision in the quadric.
			/// [JP] 平面方程式を構築: normal · point + w = 0。
			///      w = -(normal · v0) とすることで v0 が正確に平面上に乗る。
			///      Quadric の精度のため Double に昇格する。
			Double normalX = normal.x;
			Double normalY = normal.y;
			Double normalZ = normal.z;
			Double planeW = -(normalX * positions[index0].x + normalY * positions[index0].y + normalZ * positions[index0].z);

			/// [EN] Create a quadric for this plane and add it to all 3 vertices.
			///      Each vertex accumulates the quadrics of ALL its adjacent triangles.
			/// [JP] この平面の Quadric を作り、3 頂点すべてに加算する。
			///      各頂点は隣接する全三角形の Quadric を累積する。
			Quadric quadric;
			quadric.FromPlane(normalX, normalY, normalZ, planeW);
			quadrics[index0] += quadric;
			quadrics[index1] += quadric;
			quadrics[index2] += quadric;

			/// [EN] Record which triangles each vertex belongs to (adjacency list).
			/// [JP] 各頂点がどの三角形に属するかを記録する（隣接リスト）。
			vertexTriangles[index0].push_back(static_cast<Uint32>(triangleIndex));
			vertexTriangles[index1].push_back(static_cast<Uint32>(triangleIndex));
			vertexTriangles[index2].push_back(static_cast<Uint32>(triangleIndex));
		}

		/// [EN] PHASE 2: Build the edge priority queue.
		///
		///      EdgeEntry stores all the information needed to collapse an edge:
		///      - vertex0, vertex1: the two endpoints
		///      - cost: the quadric error at the optimal merged position
		///      - optimalPosition: where the merged vertex should go
		///      - version: used for "lazy deletion" (see below)
		///
		///      "Lazy deletion" in priority queues:
		///      When we collapse an edge, the neighbouring edges' costs change.
		///      Instead of removing old entries from the heap (which is O(n)),
		///      we just push NEW entries with an incremented version number.
		///      When we pop an entry, we check if its version matches the
		///      current version for that edge. If not, the entry is stale and
		///      we skip it. This turns O(n) removal into O(1) skip.
		/// [JP] フェーズ 2: エッジ優先度キューを構築する。
		///
		///      EdgeEntry はエッジの崩壊に必要な全情報を格納する:
		///      - vertex0, vertex1: 2 つの端点
		///      - cost: 最適統合位置での Quadric 誤差
		///      - optimalPosition: 統合後の頂点を配置すべき位置
		///      - version: 「遅延削除」に使う（下記参照）
		///
		///      優先度キューの「遅延削除」:
		///      エッジを崩壊すると、隣接エッジのコストが変化する。
		///      古いエントリをヒープから除去するのは O(n) なので、代わりに
		///      バージョン番号をインクリメントした新しいエントリを push する。
		///      エントリを pop したとき、そのエッジの現在のバージョンと一致するか
		///      確認する。一致しなければ古いエントリなのでスキップする。
		///      これにより O(n) の除去が O(1) のスキップになる。
		struct EdgeEntry
		{
			Uint32 vertex0;
			Uint32 vertex1;
			Double cost;
			Vector3 optimalPosition;
			Uint32 version;
			Bool operator>(const EdgeEntry& other)const { return cost > other.cost; }
		};

		/// [EN] Converts an edge (vertexA, vertexB) into a single Uint64 key by
		///      always putting the smaller index in the upper 32 bits. This ensures
		///      edge(3,7) and edge(7,3) produce the same key.
		/// [JP] エッジ (vertexA, vertexB) を単一の Uint64 キーに変換する。
		///      常に小さい方のインデックスを上位 32 ビットに置くことで、
		///      edge(3,7) と edge(7,3) が同じキーになるようにする。
		auto makeEdgeKey = [](Uint32 vertexA, Uint32 vertexB) -> Uint64
			{
				if (vertexA > vertexB)
				{
					std::swap(vertexA, vertexB);
				}
				return (static_cast<Uint64>(vertexA) << 32) | vertexB;
			};

		/// [EN] Maps edge keys to their current version number for lazy deletion.
		/// [JP] 遅延削除のため、エッジキーから現在のバージョン番号へのマップ。
		std::unordered_map<Uint64, Uint32> edgeVersions;

		/// [EN] Computes the collapse cost for an edge and returns a complete EdgeEntry.
		///
		///      The cost depends on whether the endpoints are locked:
		///      - Both unlocked: use the optimal position from OptimalPosition().
		///      - One locked:    force the merged vertex to stay at the locked endpoint.
		///      - Both locked:   the edge cannot be collapsed (cost = 1e30).
		///
		///      The version is incremented each time an edge's cost is recalculated,
		///      so stale heap entries can be detected and skipped.
		///
		/// [JP] エッジの崩壊コストを計算し、完全な EdgeEntry を返す。
		///
		///      コストは端点のロック状態に依存する:
		///      - 両方アンロック: OptimalPosition() からの最適位置を使う。
		///      - 片方ロック:     統合頂点をロックされた端点に固定する。
		///      - 両方ロック:     崩壊不可能（コスト = 1e30）。
		///
		///      バージョンはエッジのコストが再計算されるたびにインクリメントされ、
		///      ヒープ内の古いエントリを検出してスキップできるようにする。
		auto calculateEdgeCost = [&](Uint32 vertex0, Uint32 vertex1) -> EdgeEntry
			{
				EdgeEntry entry;
				entry.vertex0 = vertex0;
				entry.vertex1 = vertex1;

				/// [EN] Combine the quadrics of both endpoints. The combined quadric
				///      represents the total error to ALL planes from both vertices.
				/// [JP] 両端点の Quadric を合成する。合成された Quadric は
				///      両方の頂点からの全平面に対する合計誤差を表す。
				Quadric combinedQuadric = quadrics[vertex0];
				combinedQuadric += quadrics[vertex1];

				Bool vertex0Locked = locked && locked[vertex0];
				Bool vertex1Locked = locked && locked[vertex1];

				Double optimalX, optimalY, optimalZ;
				if (!vertex0Locked && !vertex1Locked && combinedQuadric.OptimalPosition(optimalX, optimalY, optimalZ))
				{
					/// [EN] Best case: both unlocked and the optimal position was found.
					///      The cost is the error at that optimal position.
					/// [JP] 最良のケース: 両方アンロックで最適位置が見つかった。
					///      コストはその最適位置での誤差。
					entry.optimalPosition = Vector3(static_cast<Float>(optimalX), static_cast<Float>(optimalY), static_cast<Float>(optimalZ));
					entry.cost = combinedQuadric.Evaluate(optimalX, optimalY, optimalZ);
				}
				else if (vertex0Locked && !vertex1Locked)
				{
					/// [EN] vertex0 is locked: the merged vertex must stay at vertex0's position.
					/// [JP] vertex0 がロック: 統合頂点は vertex0 の位置に留まらなければならない。
					entry.optimalPosition = positions[vertex0];
					entry.cost = combinedQuadric.Evaluate(positions[vertex0].x, positions[vertex0].y, positions[vertex0].z);
				}
				else if (!vertex0Locked && vertex1Locked)
				{
					/// [EN] vertex1 is locked: the merged vertex must stay at vertex1's position.
					/// [JP] vertex1 がロック: 統合頂点は vertex1 の位置に留まらなければならない。
					entry.optimalPosition = positions[vertex1];
					entry.cost = combinedQuadric.Evaluate(positions[vertex1].x, positions[vertex1].y, positions[vertex1].z);
				}
				else
				{
					/// [EN] Both locked OR OptimalPosition failed: assign huge cost to prevent collapse.
					/// [JP] 両方ロック、または OptimalPosition 失敗: 崩壊を防ぐため巨大なコストを割り当て。
					entry.optimalPosition = positions[vertex0];
					entry.cost = 1e30;
				}

				/// [EN] Floating-point rounding can produce tiny negative costs; clamp to zero.
				/// [JP] 浮動小数点の丸め誤差で微小な負のコストが生じうるのでゼロにクランプ。
				if (entry.cost < 0.0)
				{
					entry.cost = 0.0;
				}

				/// [EN] Bump the version for this edge so any older heap entries become stale.
				/// [JP] このエッジのバージョンを上げ、ヒープ内の古いエントリを無効にする。
				Uint64 key = makeEdgeKey(vertex0, vertex1);
				entry.version = ++edgeVersions[key];
				return entry;
			};

		/// [EN] Min-heap: the edge with the LOWEST cost is at the top (std::greater reverses default max-heap).
		/// [JP] ミニヒープ: 最小コストのエッジが先頭にくる（std::greater でデフォルトのマックスヒープを反転）。
		std::priority_queue<EdgeEntry, DynamicArray<EdgeEntry>, std::greater<EdgeEntry>> edgeHeap;

		/// [EN] Enumerate all unique edges from the triangle list and push their initial costs.
		///      seenEdges prevents the same edge from being pushed twice (shared by two triangles).
		/// [JP] 三角形リストから全ての一意なエッジを列挙し、初期コストを push する。
		///      seenEdges により同じエッジが 2 回 push されるのを防ぐ（2 つの三角形が共有するため）。
		{
			std::set<Uint64> seenEdges;
			for (Size triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
			{
				Uint32 triangleVertices[3] =
				{
					indices[triangleIndex * 3],
					indices[triangleIndex * 3 + 1],
					indices[triangleIndex * 3 + 2]
				};
				/// [EN] Each triangle has 3 edges: (v0,v1), (v1,v2), (v2,v0).
				/// [JP] 各三角形は 3 つのエッジを持つ: (v0,v1), (v1,v2), (v2,v0)。
				for (Int edgeIndex = 0; edgeIndex < 3; edgeIndex++)
				{
					Uint64 key = makeEdgeKey(triangleVertices[edgeIndex], triangleVertices[(edgeIndex + 1) % 3]);
					if (seenEdges.insert(key).second)
					{
						edgeHeap.push(calculateEdgeCost(triangleVertices[edgeIndex], triangleVertices[(edgeIndex + 1) % 3]));
					}
				}
			}
		}

		/// [EN] PHASE 3: Greedy edge collapse loop.
		///      Repeatedly collapse the cheapest valid edge until we reach
		///      the target triangle count or run out of collapsible edges.
		/// [JP] フェーズ 3: 貪欲なエッジ崩壊ループ。
		///      最小コストの有効なエッジを繰り返し崩壊させ、目標三角形数に
		///      到達するか、崩壊可能なエッジがなくなるまで続ける。
		Size aliveTriangleCount = triangleCount - degenerateTriangleCount;

		while (aliveTriangleCount > targetTriangleCount && !edgeHeap.empty())
		{
			EdgeEntry top = edgeHeap.top();
			edgeHeap.pop();

			/// [EN] LAZY DELETION CHECK 1: Is this entry stale?
			///      If the edge has been recalculated since this entry was pushed,
			///      a newer entry exists in the heap with a higher version number.
			///      Skip this outdated one.
			/// [JP] 遅延削除チェック 1: このエントリは古いか？
			///      このエントリが push された後にエッジが再計算されていれば、
			///      より高いバージョン番号の新しいエントリがヒープにある。
			///      この古いものはスキップする。
			Uint64 key = makeEdgeKey(top.vertex0, top.vertex1);
			if (edgeVersions[key] != top.version)
			{
				continue;
			}

			/// [EN] LAZY DELETION CHECK 2: Are the endpoints still alive?
			///      A previous collapse may have already killed one of them.
			/// [JP] 遅延削除チェック 2: 端点はまだ生きているか？
			///      以前の崩壊で既にどちらかが削除されている可能性がある。
			if (vertexDead[top.vertex0] || vertexDead[top.vertex1])
			{
				continue;
			}

			/// [EN] If the cheapest remaining edge has enormous cost (>= 1e29),
			///      it means all remaining edges are locked or uncollapsible. Stop.
			/// [JP] 残りの最小コストエッジが巨大なコスト (>= 1e29) なら、
			///      残りのエッジはすべてロック済みか崩壊不可能。停止する。
			if (top.cost >= 1e29)
			{
				break;
			}

			/// [EN] Decide which vertex survives (keepVertex) and which is removed (killVertex).
			///      Default: keep vertex0, kill vertex1.
			///      Exception: if vertex1 is locked and vertex0 is not, swap them
			///      so the locked vertex survives (its position is preserved).
			/// [JP] どちらの頂点を生かし（keepVertex）、どちらを削除するか（killVertex）を決定。
			///      デフォルト: vertex0 を生かし、vertex1 を削除。
			///      例外: vertex1 がロックで vertex0 がアンロックなら入れ替え、
			///      ロックされた頂点が生き残る（位置が保持される）ようにする。
			Uint32 keepVertex = top.vertex0;
			Uint32 killVertex = top.vertex1;
			if (locked && locked[killVertex] && !locked[keepVertex])
			{
				std::swap(keepVertex, killVertex);
			}

			/// [EN] Move the surviving vertex to the optimal position and absorb the
			///      killed vertex's quadric (so future collapses account for its planes).
			/// [JP] 生き残る頂点を最適位置に移動し、削除される頂点の Quadric を吸収する
			///     （将来の崩壊でその平面が考慮されるようにする）。
			positions[keepVertex] = top.optimalPosition;
			quadrics[keepVertex] += quadrics[killVertex];
			vertexDead[killVertex] = true;

			/// [EN] Collect all vertices that are now neighbours of keepVertex (after rewriting).
			///      We need this to re-cost the affected edges afterwards.
			/// [JP] 書き換え後に keepVertex の隣接頂点となるすべての頂点を収集する。
			///      影響を受けたエッジのコストを再計算するために必要。
			std::set<Uint32> neighborVertices;

			/// [EN] Iterate over all triangles that referenced killVertex.
			///      For each such triangle, rewrite killVertex → keepVertex in the index buffer.
			/// [JP] killVertex を参照していた全三角形について反復する。
			///      各三角形のインデックスバッファで killVertex → keepVertex に書き換える。
			for (Uint32 triangleIndex : vertexTriangles[killVertex])
			{
				if (triangleDead[triangleIndex])
				{
					continue;
				}

				Uint32& indexA = indices[triangleIndex * 3];
				Uint32& indexB = indices[triangleIndex * 3 + 1];
				Uint32& indexC = indices[triangleIndex * 3 + 2];

				/// [EN] Replace every occurrence of killVertex with keepVertex in this triangle.
				/// [JP] この三角形内の killVertex の出現をすべて keepVertex に置き換える。
				if (indexA == killVertex)
				{
					indexA = keepVertex;
				}
				if (indexB == killVertex)
				{
					indexB = keepVertex;
				}
				if (indexC == killVertex)
				{
					indexC = keepVertex;
				}

				/// [EN] After rewriting, check if the triangle became degenerate
				///      (two or more vertices are the same). A degenerate triangle has
				///      zero area and must be removed.
				///
				///      This typically happens to the two triangles that shared the
				///      collapsed edge — they had both keepVertex and killVertex as
				///      vertices, and now they have keepVertex twice.
				///
				/// [JP] 書き換え後、三角形が退化したか確認する（2 つ以上の頂点が同じ）。
				///      退化三角形は面積ゼロなので除去する必要がある。
				///
				///      これは通常、崩壊したエッジを共有していた 2 つの三角形で
				///      発生する。それらは keepVertex と killVertex の両方を頂点に
				///      持っていたが、今は keepVertex が 2 つになっている。
				if (indexA == indexB || indexB == indexC || indexA == indexC)
				{
					triangleDead[triangleIndex] = true;
					aliveTriangleCount--;
				}
				else
				{
					/// [EN] Triangle survived — transfer it to keepVertex's adjacency list
					///      and record its vertices as neighbours for edge re-costing.
					/// [JP] 三角形は生存 — keepVertex の隣接リストに移し、
					///      エッジ再計算のためにその頂点を隣接頂点として記録する。
					vertexTriangles[keepVertex].push_back(triangleIndex);
					neighborVertices.insert(indexA);
					neighborVertices.insert(indexB);
					neighborVertices.insert(indexC);
				}
			}

			/// [EN] Re-cost all edges from keepVertex to its neighbours. This pushes
			///      new entries into the heap with updated costs and incremented versions,
			///      making the old entries stale (lazy deletion).
			/// [JP] keepVertex からすべての隣接頂点へのエッジを再計算する。
			///      更新されたコストとインクリメントされたバージョンの新しいエントリを
			///      ヒープに push し、古いエントリを無効にする（遅延削除）。
			for (Uint32 neighborVertex : neighborVertices)
			{
				if (neighborVertex != keepVertex && !vertexDead[neighborVertex])
				{
					edgeHeap.push(calculateEdgeCost(keepVertex, neighborVertex));
				}
			}
		}

		/// [EN] PHASE 4: Compact the result.
		///      Build a dense output by collecting only surviving vertices and
		///      triangles. remapTable translates old vertex indices to new ones.
		///      vertexMap records which original vertex each output vertex
		///      corresponds to, so the caller can copy attributes (normals, UVs).
		/// [JP] フェーズ 4: 結果をコンパクト化する。
		///      生き残った頂点と三角形だけを収集して密な出力を構築する。
		///      remapTable は古い頂点インデックスを新しいものに変換する。
		///      vertexMap は各出力頂点が元のどの頂点に対応するかを記録し、
		///      呼び出し側が属性（法線、UV）をコピーできるようにする。
		Result result;

		/// [EN] remapTable[oldIndex] = newIndex. 0xFFFFFFFF means the vertex was killed.
		/// [JP] remapTable[旧インデックス] = 新インデックス。0xFFFFFFFF は頂点が削除されたことを示す。
		DynamicArray<Uint32> remapTable(vertexCount, 0xFFFFFFFF);

		for (Size index = 0; index < vertexCount; index++)
		{
			if (!vertexDead[index])
			{
				remapTable[index] = static_cast<Uint32>(result.positions.size());
				result.positions.push_back(positions[index]);
				result.vertexMap.push_back(static_cast<Uint32>(index));
			}
		}

		/// [EN] Remap the indices of surviving triangles to the new vertex numbering.
		/// [JP] 生き残った三角形のインデックスを新しい頂点番号に変換する。
		for (Size triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
		{
			if (triangleDead[triangleIndex])
			{
				continue;
			}
			result.indices.push_back(remapTable[indices[triangleIndex * 3]]);
			result.indices.push_back(remapTable[indices[triangleIndex * 3 + 1]]);
			result.indices.push_back(remapTable[indices[triangleIndex * 3 + 2]]);
		}

		return result;
	}
}
