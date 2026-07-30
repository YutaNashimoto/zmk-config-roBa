#define DT_DRV_COMPAT zmk_behavior_ctrl_mo

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// behavior_cmd_mo.c で定義。保持中の修飾キー（MOD_* のビットフラグ）を共有する。
extern zmk_mod_flags_t cmd_mo_held_mod;
// 修飾キーをHIDへ直接登録/排出する共通ヘルパー（behavior_cmd_mo.c で定義）。
extern void roba_mod_hold(zmk_mod_flags_t mod_flag);
extern void roba_mod_release(zmk_mod_flags_t mod_flag);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// cmd_mo の Windows 版。RCTRL を保持しつつレイヤーを一時有効化する。
static int ctrl_mo_pressed(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    cmd_mo_held_mod = MOD_RCTL;
    zmk_keymap_layer_activate(binding->param1);
    roba_mod_hold(MOD_RCTL);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int ctrl_mo_released(struct zmk_behavior_binding *binding,
                            struct zmk_behavior_binding_event event) {
    roba_mod_release(MOD_RCTL);
    zmk_keymap_layer_deactivate(binding->param1);
    cmd_mo_held_mod = 0;
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
// キーマップエディタ/ZMK Studio で param1 を「レイヤー」ピッカーとして表示させる。
static const struct behavior_parameter_value_metadata ctrl_mo_param_values[] = {
    {
        .display_name = "Layer",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_LAYER_ID,
    },
};

static const struct behavior_parameter_metadata_set ctrl_mo_param_metadata_set[] = {{
    .param1_values = ctrl_mo_param_values,
    .param1_values_len = ARRAY_SIZE(ctrl_mo_param_values),
}};

static const struct behavior_parameter_metadata ctrl_mo_metadata = {
    .sets_len = ARRAY_SIZE(ctrl_mo_param_metadata_set),
    .sets = ctrl_mo_param_metadata_set,
};
#endif

static const struct behavior_driver_api behavior_ctrl_mo_driver_api = {
    .binding_pressed = ctrl_mo_pressed,
    .binding_released = ctrl_mo_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &ctrl_mo_metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_ctrl_mo_driver_api);

#endif
