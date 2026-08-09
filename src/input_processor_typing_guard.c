/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * zip_typing_guard: 直前のキー打鍵から param1(ms) 以内のトラックボール移動
 * (REL_X/REL_Y) を「移動量0」に潰し、タイピング中の打鍵振動によるカーソル
 * 暴走を防ぐ。
 *
 * 注意: v0.3-branch の input_listener（filter_with_input_config）は、
 * process-next なしの子ノード（roBa_R.overlay の ipad/iphone/win）内で
 * ZMK_INPUT_PROC_STOP を返しても "return 0"(CONTINUE) に握り潰してしまう
 * ため、STOPでの破棄はbase(Mac)以外で機能しない。value=0方式なら
 * process-nextの有無・チェーン内の位置に関わらず一様に効く。
 */

#define DT_DRV_COMPAT zmk_input_processor_typing_guard

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_REGISTER(zmk_input_processor_typing_guard, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct typing_guard_data {
    const struct device *dev;
    struct k_mutex lock;
    int64_t last_tapped_timestamp;
};

// カーソル移動(REL_X/REL_Y)以外は素通し。スクロール等は対象外。
static int typing_guard_handle_event(const struct device *dev, struct input_event *event,
                                     uint32_t param1, uint32_t param2,
                                     struct zmk_input_processor_state *state) {
    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    struct typing_guard_data *data = (struct typing_guard_data *)dev->data;

    int ret = k_mutex_lock(&data->lock, K_FOREVER);
    if (ret < 0) {
        return ret;
    }
    int64_t last = data->last_tapped_timestamp;
    k_mutex_unlock(&data->lock);

    // param1 = ガード窓(ms)。直前打鍵から窓内なら移動量を0にする。
    const int32_t raw = event->value;
    bool suppressed = false;
    if (param1 > 0 && (last + (int64_t)param1) > k_uptime_get()) {
        event->value = 0;
        suppressed = true;
    }

    // 【デバッグ用・カーソル暴走の切り分け】ドライバが出力した生の移動量を記録する。
    // このプロセッサは全チェーンの先頭にあるため、ここに出る値は「ZMKの後続処理を
    // 一切通っていない、ドライバ直後の値」である。ボールを固定した状態でここに値が
    // 流れるなら発生源はドライバ/センサー/配線側で、後続のZMK処理は無罪と確定する。
    // 診断が済んだらこのログと CONFIG_ZMK_USB_LOGGING は削除すること。
    if (raw != 0) {
        LOG_INF("trackball %s raw=%d suppressed=%d", event->code == INPUT_REL_X ? "X" : "Y", raw,
                suppressed ? 1 : 0);
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api typing_guard_driver_api = {
    .handle_event = typing_guard_handle_event,
};

// 打鍵時刻の記録（zip_temp_layerと同じ購読方式）。押下・離しの両方で更新。
static int typing_guard_keycode_listener(const struct device *dev, const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct typing_guard_data *data = (struct typing_guard_data *)dev->data;

    int ret = k_mutex_lock(&data->lock, K_FOREVER);
    if (ret < 0) {
        return ret;
    }
    data->last_tapped_timestamp = ev->timestamp;
    k_mutex_unlock(&data->lock);

    return ZMK_EV_EVENT_BUBBLE;
}

#define TYPING_GUARD_DISPATCH(inst)                                                                \
    {                                                                                              \
        int err = typing_guard_keycode_listener(DEVICE_DT_INST_GET(inst), eh);                     \
        if (err < 0) {                                                                             \
            return err;                                                                            \
        }                                                                                          \
    }

static int typing_guard_event_dispatcher(const zmk_event_t *eh) {
    DT_INST_FOREACH_STATUS_OKAY(TYPING_GUARD_DISPATCH)
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(processor_typing_guard, typing_guard_event_dispatcher);
ZMK_SUBSCRIPTION(processor_typing_guard, zmk_keycode_state_changed);

static int typing_guard_init(const struct device *dev) {
    struct typing_guard_data *data = (struct typing_guard_data *)dev->data;
    data->dev = dev;
    data->last_tapped_timestamp = 0;
    k_mutex_init(&data->lock);
    return 0;
}

#define TYPING_GUARD_INST(n)                                                                       \
    static struct typing_guard_data processor_typing_guard_data_##n = {};                          \
    DEVICE_DT_INST_DEFINE(n, typing_guard_init, NULL, &processor_typing_guard_data_##n, NULL,      \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                         \
                          &typing_guard_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TYPING_GUARD_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
