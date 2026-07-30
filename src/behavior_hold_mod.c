#define DT_DRV_COMPAT zmk_behavior_hold_mod

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zmk/keys.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// 修飾キーをHIDへ直接登録/排出する共通ヘルパー（behavior_cmd_mo.c で定義）。
extern void roba_mod_hold(zmk_mod_flags_t mod_flag);
extern void roba_mod_release(zmk_mod_flags_t mod_flag);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// 修飾キーのキーコード(LCMD/LCTRL等)を MOD_* のビットフラグへ変換する。
// キーコードのマクロで直接分岐する（HID usage の定数名は LEFTCONTROL と
// LEFT_GUI で命名が不統一なため、算術で導出せず素直に列挙する）。
// 修飾キー以外が渡された場合は0を返し、呼び出し側で何もしない。
static zmk_mod_flags_t mod_flag_from_keycode(uint32_t encoded) {
    switch (encoded) {
    case LEFT_CONTROL:
        return MOD_LCTL;
    case LEFT_SHIFT:
        return MOD_LSFT;
    case LEFT_ALT:
        return MOD_LALT;
    case LEFT_GUI:
        return MOD_LGUI;
    case RIGHT_CONTROL:
        return MOD_RCTL;
    case RIGHT_SHIFT:
        return MOD_RSFT;
    case RIGHT_ALT:
        return MOD_RALT;
    case RIGHT_GUI:
        return MOD_RGUI;
    default:
        return 0;
    }
}

// hold-tap の hold 側に置いて使う、修飾キー保持専用の behavior。
// &kp と違い keycode_state_changed を経由せずHIDへ直接登録するため、hold-tapの
// 保留・再送による参照カウントの重複加算が起きない。離すときはカウンタを0まで
// 排出するので、万一別経路で余分な加算が入っていても修飾キーが固着しない。
// （背景の詳細は behavior_cmd_mo.c のコメントを参照）
static int hold_mod_pressed(struct zmk_behavior_binding *binding,
                            struct zmk_behavior_binding_event event) {
    roba_mod_hold(mod_flag_from_keycode(binding->param1));
    return ZMK_BEHAVIOR_OPAQUE;
}

static int hold_mod_released(struct zmk_behavior_binding *binding,
                             struct zmk_behavior_binding_event event) {
    roba_mod_release(mod_flag_from_keycode(binding->param1));
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
// キーマップエディタ/ZMK Studio で param1 を「キーコード」ピッカーとして表示させる。
static const struct behavior_parameter_value_metadata hold_mod_param_values[] = {
    {
        .display_name = "Modifier",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_HID_USAGE,
    },
};

static const struct behavior_parameter_metadata_set hold_mod_param_metadata_set[] = {{
    .param1_values = hold_mod_param_values,
    .param1_values_len = ARRAY_SIZE(hold_mod_param_values),
}};

static const struct behavior_parameter_metadata hold_mod_metadata = {
    .sets_len = ARRAY_SIZE(hold_mod_param_metadata_set),
    .sets = hold_mod_param_metadata_set,
};
#endif

static const struct behavior_driver_api behavior_hold_mod_driver_api = {
    .binding_pressed = hold_mod_pressed,
    .binding_released = hold_mod_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &hold_mod_metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_hold_mod_driver_api);

#endif
