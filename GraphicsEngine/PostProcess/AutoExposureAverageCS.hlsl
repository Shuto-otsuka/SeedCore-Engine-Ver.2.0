#include "../Shader/Constants.hlsli"

/**
* [EN]
* Reference:
* - https://bruop.github.io/exposure/
* - https://knarkowicz.wordpress.com/2016/01/09/automatic-exposure/
*
* Reduces the 256-bin histogram (built by AutoExposureHistogramCS.hlsl this
* same frame) into a target exposure value, then blends the persistent
* smoothed exposure toward it - a single thread, dispatched (1,1,1), since
* this runs once per view per frame and the reduction cost is negligible next
* to the histogram build.
*
* Bin 0 (near-black, see the histogram shader) is excluded from the weighted
* average. avg_log_luminance is converted to a target EV via the classic
* photographic relation exposure = key_value / avg_luminance, i.e.
* target_ev = log2(key_value) - avg_log_luminance, then clamped to a sane range so
* a nearly-empty histogram (e.g. the very first frame, or a pure-black
* render) cannot send exposure to +-infinity.
*
* Temporal adaptation blends the persistent value toward target_ev with an
* exponential decay (1 - exp(-delta_time_ * speed)), using ExposureSettings'
* two asymmetric speeds - see AdaptSpeed's doc comment in PostProcess.h for
* why brightening and darkening adapt at different rates. The persistent
* buffer (constant_indices.post_process_.exposure_.exposure_uav_index_) is a single
* float that PostProcessRenderer never clears after its one-time zero
* initialization - reading and writing it here IS the adaptation state
* carrying across frames.
*
* ---------------------------------------------------------------------
*
* [JP]
* 同じフレーム内で AutoExposureHistogramCS.hlsl が構築した 256 ビンの
* ヒストグラムを縮約して目標露出値を求め、永続化された平滑化露出値をそこへ
* ブレンドする — ビューごとに1フレーム1回、(1,1,1) ディスパッチのシングル
* スレッドで十分(縮約のコストはヒストグラム構築に比べて無視できる)。
*
* ビン0(ほぼ黒、ヒストグラムシェーダ参照)は加重平均から除外する。
* avg_log_luminance は、写真の古典的な関係式
* exposure = key_value / avg_luminance すなわち
* target_ev = log2(key_value) - avg_log_luminance で目標EVへ変換し、ほぼ空の
* ヒストグラム(起動直後の最初のフレームや真っ黒な描画など)で露出が±無限大へ
* 飛ばないよう妥当な範囲へクランプする。
*
* 時間順応は、永続値を target_ev へ指数的減衰(1 - exp(-delta_time_ * speed))で
* ブレンドする。ExposureSettings の非対称な2つの速度を使う理由は
* PostProcess.h の AdaptSpeed 系フィールドのコメント参照。永続バッファ
* (constant_indices.post_process_.exposure_.exposure_uav_index_)は単一の float で、
* PostProcessRenderer は初回のゼロ初期化以降クリアしない — ここでの読み書き
* そのものが、フレームを跨いだ順応状態の保持になっている。
*/

[numthreads(1, 1, 1)]
void main()
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	RWStructuredBuffer<uint> histogram = ResourceDescriptorHeap[constant_indices.post_process_.exposure_.histogram_uav_index_];

	float weighted_log_sum = 0.0;
	float total_count = 0.0;

	for (uint bin = 1; bin < 256; bin++)
	{
		float count = float(histogram[bin]);
		weighted_log_sum += count * (float(bin - 1) / 254.0);
		total_count += count;
	}

	float min_log = constant_indices.post_process_.exposure_.min_log_luminance_;
	float max_log = constant_indices.post_process_.exposure_.max_log_luminance_;

	float avg_log_luminance = total_count > 0.0
		? (weighted_log_sum / total_count) * (max_log - min_log) + min_log
		: min_log;

	float key_value = constant_indices.post_process_.exposure_.key_value_;
	float target_ev = clamp(log2(max(key_value, 0.0001)) - avg_log_luminance, -8.0, 8.0);

	RWStructuredBuffer<float> exposure = ResourceDescriptorHeap[constant_indices.post_process_.exposure_.exposure_uav_index_];
	float smoothed_ev = exposure[0];

	// 目標EVが下がった = シーンが明るくなった = 明順応(速い方の速度)。
	bool brightening = target_ev < smoothed_ev;
	float speed = brightening ? constant_indices.post_process_.exposure_.adapt_speed_to_bright_ : constant_indices.post_process_.exposure_.adapt_speed_to_dark_;
	float blend = 1.0 - exp(-scene.delta_time_ * max(speed, 0.0001));

	smoothed_ev += (target_ev - smoothed_ev) * blend;
	exposure[0] = smoothed_ev;
}
