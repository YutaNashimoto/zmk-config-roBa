#define DT_DRV_COMPAT zmk_behavior_to_base_keep_cpi

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/keymap.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// 精密モード層(CHANGE_CPI)。&to による全レイヤークリアから保護する層番号。
// レイヤー構成を変更したら必ずここも更新すること（現在: 28）。
#define CHANGE_CPI_LAYER 28

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// &to と同じく「他の全レイヤーをクリアして param1 のレイヤーへ戻る」挙動だが、
// 精密モード層(CHANGE_CPI)だけは元の ON/OFF 状態を維持する。
// これにより、マウス層の各キー(mt_to_0 / to_layer_0 等)で通常操作へ復帰しても
// 精密モードのトグルが巻き添えで解除されない（DPIボタン的な永続トグル）。
static int to_base_keep_cpi_pressed(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    bool cpi_was_on = zmk_keymap_layer_active(CHANGE_CPI_LAYER);
    zmk_keymap_layer_to(binding->param1);
    if (cpi_was_on) {
        zmk_keymap_layer_activate(CHANGE_CPI_LAYER);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static int to_base_keep_cpi_released(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_to_base_keep_cpi_driver_api = {
    .binding_pressed = to_base_keep_cpi_pressed,
    .binding_released = to_base_keep_cpi_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_to_base_keep_cpi_driver_api);

#endif
