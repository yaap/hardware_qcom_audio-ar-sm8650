/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <system/audio.h>

namespace oprec {

enum class Scene {
    DEFAULT,
    INTERVIEW,
    CONFERENCE,
    APP1,
};

struct Capture {
    audio_source_t source;
    audio_devices_t device;
    uint32_t sample_rate;
    uint32_t channels;
    Scene scene;
    int effect_type = -1;
    bool low_latency = false;
};

struct Selection {
    // Vendor profile ID, or -1 when OPREC must not process this capture.
    int32_t profile;
    // Vendor record type passed to the wrapper's create_instance().
    uint32_t record_type;
};

}  // namespace oprec
