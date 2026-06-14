/*
 * BLEプロファイル番号を機器名に反映する
 *
 * アクティブなBluetoothプロファイルが変わるたびに、機器名を
 * "<prefix>_<番号>"（例: profile0 -> "roBa_1"）へ動的に切り替える。
 * これによりホスト側のBluetooth一覧で、どのプロファイルに繋いで
 * いるか一目で分かるようになる。
 *
 * ZMK標準の zmk_ble_set_device_name()（bt_set_name + 広告再開）と
 * zmk_ble_active_profile_changed イベントを利用。広告は
 * BT_LE_ADV_OPT_USE_NAME 付きなので、新しい名前が自動で載る。
 *
 * 注意: 既にペアリング済みのホストは旧名をキャッシュしているため、
 * 表示を更新するには各プロファイルを再ペアリングする必要がある。
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_REGISTER(ble_profile_name, CONFIG_ZMK_LOG_LEVEL);

static char name_buf[CONFIG_BT_DEVICE_NAME_MAX + 1];

static int profile_name_listener(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *ev = as_zmk_ble_active_profile_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* プロファイル番号は1始まりで表示（index 0 -> "roBa_1"） */
    snprintf(name_buf, sizeof(name_buf), "%s_%d", CONFIG_ZMK_BLE_PROFILE_NAME_PREFIX,
             ev->index + 1);

    int err = zmk_ble_set_device_name(name_buf);
    if (err) {
        LOG_ERR("Failed to set device name for profile %d (err %d)", ev->index, err);
    } else {
        LOG_INF("BLE device name set to %s", name_buf);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(ble_profile_name, profile_name_listener);
ZMK_SUBSCRIPTION(ble_profile_name, zmk_ble_active_profile_changed);
