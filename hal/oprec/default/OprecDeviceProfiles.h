/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <OprecProfileConfig.h>

namespace oprec {

inline Selection selectProfile(const Capture& capture) {
    uint32_t record_type = capture.low_latency ? 1 : 0;
    /* The stock HAL only uses the camcorder record types for >2 channels. */
    if (capture.source == AUDIO_SOURCE_CAMCORDER && capture.channels > 2) {
        record_type = 3;
    }

    const bool is_builtin = capture.device == AUDIO_DEVICE_IN_BUILTIN_MIC ||
                            capture.device == AUDIO_DEVICE_IN_BACK_MIC;
    const bool is_wired = capture.device == AUDIO_DEVICE_IN_WIRED_HEADSET;
    const bool is_usb = capture.device == AUDIO_DEVICE_IN_USB_DEVICE ||
                        capture.device == AUDIO_DEVICE_IN_USB_HEADSET;

    /*
     * Three/four channel camcorder capture selects its profile from the vendor
     * effect type, exactly like the stock OplusRecord::getThreeMicProfile().
     */
    if (is_builtin && capture.source == AUDIO_SOURCE_CAMCORDER && capture.channels > 2) {
        static const int32_t kThreeMicProfiles[] = {0, 1, 2, 3, -1, 0, 1};
        if (capture.effect_type < 0 ||
            capture.effect_type >=
                    (int32_t)(sizeof(kThreeMicProfiles) / sizeof(kThreeMicProfiles[0]))) {
            return {-1, record_type};
        }
        return {kThreeMicProfiles[capture.effect_type], record_type};
    }

    int32_t profile;
    if (is_builtin) {
        if (capture.scene == Scene::INTERVIEW) return {7, record_type};
        if (capture.scene == Scene::CONFERENCE) return {5, record_type};
        switch (capture.source) {
            case AUDIO_SOURCE_CAMCORDER:
                profile = 4;
                break;
            case AUDIO_SOURCE_VOICE_RECOGNITION:
                profile = 10;
                break;
            default:
                profile = 1;
                break;
        }
    } else if (is_wired) {
        if (capture.scene == Scene::INTERVIEW) return {17, record_type};
        if (capture.scene == Scene::CONFERENCE) return {15, record_type};
        switch (capture.source) {
            case AUDIO_SOURCE_CAMCORDER:
                profile = 14;
                break;
            case AUDIO_SOURCE_VOICE_RECOGNITION:
                profile = 20;
                break;
            default:
                profile = 11;
                break;
        }
    } else if (is_usb) {
        if (capture.scene == Scene::INTERVIEW) return {17, record_type};
        if (capture.scene == Scene::CONFERENCE) return {15, record_type};
        switch (capture.source) {
            case AUDIO_SOURCE_CAMCORDER:
                profile = 14;
                break;
            case AUDIO_SOURCE_VOICE_RECOGNITION:
                profile = 20;
                break;
            default:
                profile = 21;
                break;
        }
    } else {
        return {-1, record_type};
    }

    /* Scene 7 (app1) at 16 kHz uses the matching *_APP1 profile. */
    if (capture.scene == Scene::APP1 && capture.sample_rate == 16000) {
        profile++;
    }

    return {profile, record_type};
}

}  // namespace oprec
