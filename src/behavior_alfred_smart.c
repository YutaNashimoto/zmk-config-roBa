#define DT_DRV_COMPAT zmk_behavior_alfred_smart

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/keys.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define CHANGE_CPI_LAYER 8

// ランチャー起動(Cmd+Space/Ctrl+Space)を、アクティブなOSデフォルトレイヤー(0=Mac/1=iPad/
// 2=iPhone/3=Win)を実行時に判定して送出する。マウス層(4-7)・精密モード層(8)からでも、
// 裏で生きているOSデフォルトレイヤーを見て正しいOSへ復帰する。
// 復帰ロジックはbehavior_to_base_keep_cpi.cと同じ規則(精密モードのON/OFF状態を維持)。
static int alfred_smart_pressed(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    uint32_t key = RG(SPACE);
    int base_layer = 0;

    if (zmk_keymap_layer_active(3)) {
        key = LC(SPACE);
        base_layer = 3;
    } else if (zmk_keymap_layer_active(2)) {
        base_layer = 2;
    } else if (zmk_keymap_layer_active(1)) {
        base_layer = 1;
    }

    raise_zmk_keycode_state_changed_from_encoded(key, true, event.timestamp);
    raise_zmk_keycode_state_changed_from_encoded(key, false, event.timestamp);

    bool cpi_was_on = zmk_keymap_layer_active(CHANGE_CPI_LAYER);
    zmk_keymap_layer_to(base_layer);
    if (cpi_was_on) {
        zmk_keymap_layer_activate(CHANGE_CPI_LAYER);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int alfred_smart_released(struct zmk_behavior_binding *binding,
                                 struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_alfred_smart_driver_api = {
    .binding_pressed = alfred_smart_pressed,
    .binding_released = alfred_smart_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_alfred_smart_driver_api);

#endif
