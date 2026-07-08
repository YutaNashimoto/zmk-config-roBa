#define DT_DRV_COMPAT zmk_behavior_toggle_lshift

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/keys.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// LSHFTを押しっぱなし状態でトグルする。押すたびに反転し、離しても解除しない
// （&tog のシフト版）。エンコーダのmod-morph（encoder_msc_mac_shift_hscroll等）が
// 「その瞬間Shiftが押されているか」を見て上下スクロールを反転させる仕組みを利用し、
// 物理的にShiftキーを押さえ続けなくても横スクロール相当に切り替えられるようにする。
static bool lshift_held = false;

static int toggle_lshift_pressed(struct zmk_behavior_binding *binding,
                                 struct zmk_behavior_binding_event event) {
    lshift_held = !lshift_held;
    raise_zmk_keycode_state_changed_from_encoded(LSHFT, lshift_held, event.timestamp);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int toggle_lshift_released(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_toggle_lshift_driver_api = {
    .binding_pressed = toggle_lshift_pressed,
    .binding_released = toggle_lshift_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_toggle_lshift_driver_api);

#endif
