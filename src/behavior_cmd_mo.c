#define DT_DRV_COMPAT zmk_behavior_cmd_mo

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/keys.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// cmd_mo がホールド中に保持している修飾キー（encoded値、0=非保持）。
// behavior_nomod から参照し、割り当て済みキー押下時に一時解除/復帰する。
uint32_t cmd_mo_held_mod = 0;

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int cmd_mo_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    cmd_mo_held_mod = RGUI;
    zmk_keymap_layer_activate(binding->param1);
    raise_zmk_keycode_state_changed_from_encoded(RGUI, true, event.timestamp);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int cmd_mo_released(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    raise_zmk_keycode_state_changed_from_encoded(RGUI, false, event.timestamp);
    zmk_keymap_layer_deactivate(binding->param1);
    cmd_mo_held_mod = 0;
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_cmd_mo_driver_api = {
    .binding_pressed = cmd_mo_pressed,
    .binding_released = cmd_mo_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_cmd_mo_driver_api);

#endif
