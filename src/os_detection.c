/*
 * ホストOS判定（USB有線接続時のみ）
 *
 * USB列挙時にホストOSが発行する GET_DESCRIPTOR(String) 要求の wLength
 * パターンを指紋としてOSを推定し、デフォルトレイヤーを自動切替する。
 * 判定ヒューリスティックは QMK の os_detection 機能を参考に実装。
 * https://github.com/qmk/qmk_firmware/blob/master/quantum/os_detection.c
 *
 * 割り込み方法: Zephyr legacy USBスタックの usb_handle_standard_request()
 * は全standard要求の先頭付近で usb_handle_os_desc() を呼ぶ。これをリンカの
 * --wrap で包み、setupパケットを観測してから本物へ委譲する（挙動変更なし。
 * MS OS Descriptorは未登録のため本物は常に -ENOTSUP を返すだけ）。
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/logging/log.h>

#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>

LOG_MODULE_REGISTER(os_detection, CONFIG_ZMK_LOG_LEVEL);

enum detected_os {
    OS_UNSURE,
    OS_LINUX,
    OS_WINDOWS,
    OS_MACOS,
    OS_IOS,
};

static struct {
    uint8_t count;
    uint8_t cnt_02;
    uint8_t cnt_04;
    uint8_t cnt_ff;
    uint16_t last_wlength;
    int64_t last_req_ms;
    bool switched; /* 検出によりレイヤーを切り替えたか */
} st;

static void reset_counters(void) {
    st.count = 0;
    st.cnt_02 = 0;
    st.cnt_04 = 0;
    st.cnt_ff = 0;
    st.last_wlength = 0;
}

/* QMK os_detection と同等の分類規則 */
static enum detected_os classify(void) {
    if (st.count < 3) {
        return OS_UNSURE;
    }
    if (st.cnt_ff >= 2 && st.cnt_04 >= 1) {
        return OS_WINDOWS;
    }
    if (st.count == st.cnt_ff) {
        return OS_LINUX;
    }
    if (st.count >= 5 && st.last_wlength == 0xFF && st.cnt_ff >= 1 && st.cnt_02 >= 2) {
        return OS_MACOS;
    }
    if (st.count == 4 && st.cnt_ff == 0 && st.cnt_02 == 2) {
        return OS_IOS;
    }
    if (st.cnt_ff == 0 && st.cnt_02 == 3 && st.cnt_04 == 1) {
        return OS_LINUX; /* PS5 */
    }
    if (st.cnt_ff >= 1 && st.cnt_02 == 0 && st.cnt_04 == 0) {
        return OS_LINUX; /* Quest 2 等 */
    }
    return OS_UNSURE;
}

static int layer_for(enum detected_os os) {
    switch (os) {
    case OS_MACOS:
        return CONFIG_ZMK_OS_DETECTION_LAYER_MACOS;
    case OS_IOS:
        return CONFIG_ZMK_OS_DETECTION_LAYER_IOS;
    case OS_WINDOWS:
        return CONFIG_ZMK_OS_DETECTION_LAYER_WINDOWS;
    case OS_LINUX:
        return CONFIG_ZMK_OS_DETECTION_LAYER_LINUX;
    default:
        return -1;
    }
}

static void detection_work_cb(struct k_work *work) {
    enum detected_os os = classify();
    LOG_INF("OS detection: count=%d 02=%d 04=%d ff=%d last=0x%02x -> %d", st.count, st.cnt_02,
            st.cnt_04, st.cnt_ff, st.last_wlength, os);

    if (os == OS_UNSURE) {
        return;
    }

    if (zmk_usb_get_conn_state() == ZMK_USB_CONN_NONE) {
        return;
    }

    int layer = layer_for(os);
    if (layer >= 0) {
        LOG_INF("Switching default layer to %d", layer);
        zmk_keymap_layer_to(layer);
        st.switched = true;
    }
}

static K_WORK_DELAYABLE_DEFINE(detection_work, detection_work_cb);

/* USB割り込み/スタックコンテキストから呼ばれるため軽量処理のみ */
static void record_wlength(uint16_t wlength) {
    int64_t now = k_uptime_get();

    /* 前回要求から間が空いていたら再列挙とみなしてリセット */
    if (st.count > 0 && (now - st.last_req_ms) > 2000) {
        reset_counters();
    }
    st.last_req_ms = now;

    if (st.count < UINT8_MAX) {
        st.count++;
    }
    switch (wlength) {
    case 0x02:
        st.cnt_02++;
        break;
    case 0x04:
        st.cnt_04++;
        break;
    case 0xFF:
        st.cnt_ff++;
        break;
    default:
        break;
    }
    st.last_wlength = wlength;

    k_work_reschedule(&detection_work, K_MSEC(CONFIG_ZMK_OS_DETECTION_DEBOUNCE_MS));
}

extern int __real_usb_handle_os_desc(struct usb_setup_packet *setup, int32_t *len,
                                     uint8_t **data);

int __wrap_usb_handle_os_desc(struct usb_setup_packet *setup, int32_t *len, uint8_t **data) {
    if (setup != NULL && usb_reqtype_is_to_host(setup) &&
        setup->RequestType.type == USB_REQTYPE_TYPE_STANDARD &&
        setup->RequestType.recipient == USB_REQTYPE_RECIPIENT_DEVICE &&
        setup->bRequest == USB_SREQ_GET_DESCRIPTOR &&
        USB_GET_DESCRIPTOR_TYPE(setup->wValue) == USB_DESC_STRING) {
        record_wlength(setup->wLength);
    }

    return __real_usb_handle_os_desc(setup, len, data);
}

static int usb_conn_listener(const zmk_event_t *eh) {
    const struct zmk_usb_conn_state_changed *ev = as_zmk_usb_conn_state_changed(eh);

    if (ev != NULL && ev->conn_state == ZMK_USB_CONN_NONE) {
        bool was_switched = st.switched;

        k_work_cancel_delayable(&detection_work);
        reset_counters();
        st.switched = false;

#if IS_ENABLED(CONFIG_ZMK_OS_DETECTION_REVERT_ON_DISCONNECT)
        if (was_switched) {
            LOG_INF("USB disconnected, reverting to layer %d",
                    CONFIG_ZMK_OS_DETECTION_DISCONNECT_LAYER);
            zmk_keymap_layer_to(CONFIG_ZMK_OS_DETECTION_DISCONNECT_LAYER);
        }
#endif
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(os_detection, usb_conn_listener);
ZMK_SUBSCRIPTION(os_detection, zmk_usb_conn_state_changed);
