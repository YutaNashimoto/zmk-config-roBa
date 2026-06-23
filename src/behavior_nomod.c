#define DT_DRV_COMPAT zmk_behavior_nomod

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/keys.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// cmd_mo が保持中の修飾キー（behavior_cmd_mo.c で定義）。
extern uint32_t cmd_mo_held_mod;

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// cmd_mo の保持修飾を一時的に外してから param1 のキーを送る behavior。
// 押下時: 修飾を解除 → キー押下。 離す時: キー解除 → （まだホールド中なら）修飾を復帰。
// cmd_mo が離されると cmd_mo_held_mod=0 になるため、押し離し順による修飾固着を防げる。
static int nomod_pressed(struct zmk_behavior_binding *binding,
                         struct zmk_behavior_binding_event event) {
    if (cmd_mo_held_mod) {
        raise_zmk_keycode_state_changed_from_encoded(cmd_mo_held_mod, false, event.timestamp);
    }
    raise_zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int nomod_released(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    raise_zmk_keycode_state_changed_from_encoded(binding->param1, false, event.timestamp);
    if (cmd_mo_held_mod) {
        raise_zmk_keycode_state_changed_from_encoded(cmd_mo_held_mod, true, event.timestamp);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
// キーマップエディタ/ZMK Studio で param1 を「キーコード」ピッカーとして表示させる。
static const struct behavior_parameter_value_metadata nomod_param_values[] = {
    {
        .display_name = "Key",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_HID_USAGE,
    },
};

static const struct behavior_parameter_metadata_set nomod_param_metadata_set[] = {{
    .param1_values = nomod_param_values,
    .param1_values_len = ARRAY_SIZE(nomod_param_values),
}};

static const struct behavior_parameter_metadata nomod_metadata = {
    .sets_len = ARRAY_SIZE(nomod_param_metadata_set),
    .sets = nomod_param_metadata_set,
};
#endif

static const struct behavior_driver_api behavior_nomod_driver_api = {
    .binding_pressed = nomod_pressed,
    .binding_released = nomod_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &nomod_metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_nomod_driver_api);

#endif
