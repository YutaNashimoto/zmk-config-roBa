#define DT_DRV_COMPAT zmk_behavior_cmd_mo

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/modifiers.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>
#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// cmd_mo がホールド中に保持している修飾キー（MOD_* のビットフラグ、0=非保持）。
// behavior_nomod から参照し、割り当て済みキー押下時に一時解除/復帰する。
zmk_mod_flags_t cmd_mo_held_mod = 0;

// --- 修飾キーの直接操作（roBa共通ヘルパー。ctrl_mo/nomod/hold_mod から使う）---
//
// 【背景: Cmd固着バグ(再起動でしか直らない)の対策・2026-07】
// ZMKの修飾キーは explicit_modifier_counts[] という参照カウントで管理されるが、
// 「登録(register)は無制限に加算」なのに「解除(unregister)は0で頭打ち」という
// 非対称な実装になっている。しかもこの配列は static でファームウェアの寿命を
// 持ち、一括クリアするAPIも存在しない。
// 一方 hold-tap は判定待ちの間に流れてきた修飾キーイベントを保留し、判定確定後
// にまとめて再送する。roBaは文字キーがほぼ全てhold-tapなので判定待ちが常時発生
// しており、keycode_state_changed 経由で修飾キーを上げ下げすると再送のずれで
// カウンタが1回余分に加算されうる。こうなるとカウンタは二度と0に戻らず
// （キーを押し直しても +1/-1 するだけ）、Cmdが電源を切るまで固着する。
//
// そこで①イベント系を経由せずHIDへ直接登録して保留・再送の対象外にし、
// ②解除時は1減らすのではなくカウンタを0まで排出する（自己修復）。
// ②により、万一別経路で余分な加算が入っても、キーを離した時点で必ず解除される。

void roba_mod_hold(zmk_mod_flags_t mod_flag) {
    if (!mod_flag) {
        return;
    }
    zmk_hid_register_mods(mod_flag);
    zmk_endpoints_send_report(HID_USAGE_KEY);
}

void roba_mod_release(zmk_mod_flags_t mod_flag) {
    if (!mod_flag) {
        return;
    }
    // 毒されたカウンタごと0まで排出する。上限を設けて無限ループを防ぐ。
    for (int i = 0; i < 16 && (zmk_hid_get_explicit_mods() & mod_flag); i++) {
        zmk_hid_unregister_mods(mod_flag);
    }
    zmk_endpoints_send_report(HID_USAGE_KEY);
}

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int cmd_mo_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    cmd_mo_held_mod = MOD_RGUI;
    zmk_keymap_layer_activate(binding->param1);
    roba_mod_hold(MOD_RGUI);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int cmd_mo_released(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    roba_mod_release(MOD_RGUI);
    zmk_keymap_layer_deactivate(binding->param1);
    cmd_mo_held_mod = 0;
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
// キーマップエディタ/ZMK Studio で param1 を「レイヤー」ピッカーとして表示させる。
// これにより rlang（hold-tap）の hold 側パラメータがレイヤー選択式になる。
static const struct behavior_parameter_value_metadata cmd_mo_param_values[] = {
    {
        .display_name = "Layer",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_LAYER_ID,
    },
};

static const struct behavior_parameter_metadata_set cmd_mo_param_metadata_set[] = {{
    .param1_values = cmd_mo_param_values,
    .param1_values_len = ARRAY_SIZE(cmd_mo_param_values),
}};

static const struct behavior_parameter_metadata cmd_mo_metadata = {
    .sets_len = ARRAY_SIZE(cmd_mo_param_metadata_set),
    .sets = cmd_mo_param_metadata_set,
};
#endif

static const struct behavior_driver_api behavior_cmd_mo_driver_api = {
    .binding_pressed = cmd_mo_pressed,
    .binding_released = cmd_mo_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &cmd_mo_metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_cmd_mo_driver_api);

#endif
