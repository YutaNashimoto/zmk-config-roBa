/*
 * zip_drift_filter: ゆっくりした一方向のドリフト（BLE接続時に発生するカーソル暴走）
 * を遮断し、意図的な操作だけを通す入力プロセッサ。
 *
 * 【設計の根拠・2026-08の実測データ】
 * HIDレポートを実測して暴走時と通常操作を比較した結果:
 *
 *                 1イベントの|値|          レポート頻度      方向の偏り
 *   通常の操作 : 中央値2 / 90%点 8 / 最大146   47回/秒        2.3%
 *   ドリフト   : 中央値3 / 90%点13 / 最大118   10.5回/秒     42.5%
 *
 * 「1回あたりの大きさ」は両者でほぼ重なっており判別に使えない（当初は累積量で
 * 判定したが、単発で大きい値が来ると一発で閾値を超えて素通りしてしまった）。
 * 一方「単位時間あたりのイベント数」は約4.5倍の差があり、単発の大きさに影響
 * されない。そこで時間窓ごとのイベント数で判定する。
 *
 *   通常操作: 100msあたり約9.4個 → 閾値以上なので通過
 *   ドリフト: 100msあたり約2.1個 → 閾値未満なので遮断
 *
 * 一度閾値に達したら、その窓の間は通し続ける（ヒステリシス）。動きが
 * IDLE_RESET_MS 途切れたら状態をリセットする。
 *
 * 副次効果として、ドリフトでマウスレイヤー(AML)が誤起動しなくなるため、
 * 「Kが中クリックになる」「文字カーソルが飛ぶ」といった症状も止まる。
 *
 * 注意: zip_typing_guard と同じ理由で、遮断は STOP ではなく「value=0にして
 * CONTINUE」で行う。v0.3-branch の input_listener は process-next なし子ノード内で
 * STOP を握り潰すため、STOPだとOS別チェーンで効かない。
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_drift_filter

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zmk_input_processor_drift_filter, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// イベント数を数える時間窓。実測値(通常9.4個/100ms・ドリフト2.1個/100ms)から、
// 100msあれば両者に4倍以上の差がつく。
#define WINDOW_MS 100

// この時間ぶん動きが途切れたら「操作が終わった」とみなして状態をリセットする。
// ドリフトのイベント間隔(約48ms)より十分長くする必要がある。
#define IDLE_RESET_MS (WINDOW_MS * 3)

struct drift_filter_data {
    struct k_mutex lock;
    int64_t window_start; // 現在の窓の開始時刻
    int64_t last_event;   // 最後に移動イベントを受け取った時刻
    uint32_t count;       // 現在の窓で受け取ったイベント数
    bool passing;         // この窓で閾値に達したか（達したら通し続ける）
};

static int drift_filter_handle_event(const struct device *dev, struct input_event *event,
                                     uint32_t param1, uint32_t param2,
                                     struct zmk_input_processor_state *state) {
    // カーソル移動(REL_X/REL_Y)以外は素通し。スクロールは対象外。
    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    // param1 = 窓内に必要なイベント数の閾値。0ならフィルタ無効。
    if (param1 == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    struct drift_filter_data *data = (struct drift_filter_data *)dev->data;
    const int64_t now = k_uptime_get();

    int ret = k_mutex_lock(&data->lock, K_FOREVER);
    if (ret < 0) {
        return ret;
    }

    // 動きが途切れていたら操作の終わりとみなして完全にリセットする。
    if (data->last_event != 0 && (now - data->last_event) > IDLE_RESET_MS) {
        data->passing = false;
        data->count = 0;
        data->window_start = now;
    }
    data->last_event = now;

    // 窓が満了したら、その窓の結果で通過可否を確定させてから数え直す。
    // 直前の窓で閾値に達していれば継続して通す（操作中の取りこぼし防止）。
    if ((now - data->window_start) >= WINDOW_MS) {
        data->passing = (data->count >= param1);
        data->count = 0;
        data->window_start = now;
    }

    data->count++;

    // 窓の途中でも閾値に達した時点で通す（操作開始時の遅れを最小にする）。
    if (data->count >= param1) {
        data->passing = true;
    }

    const bool pass = data->passing;
    k_mutex_unlock(&data->lock);

    if (!pass) {
        event->value = 0;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api drift_filter_driver_api = {
    .handle_event = drift_filter_handle_event,
};

static int drift_filter_init(const struct device *dev) {
    struct drift_filter_data *data = (struct drift_filter_data *)dev->data;
    data->window_start = 0;
    data->last_event = 0;
    data->count = 0;
    data->passing = false;
    k_mutex_init(&data->lock);
    return 0;
}

#define DRIFT_FILTER_INST(n)                                                                       \
    static struct drift_filter_data processor_drift_filter_data_##n = {};                          \
    DEVICE_DT_INST_DEFINE(n, drift_filter_init, NULL, &processor_drift_filter_data_##n, NULL,      \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                        \
                          &drift_filter_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DRIFT_FILTER_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
