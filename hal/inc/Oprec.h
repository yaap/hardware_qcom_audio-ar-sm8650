/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>

#include <system/audio.h>

#include "PalApi.h"
#include "PalDefs.h"

class StreamInPrimary;

/*
 * OPLUS AP-side record algorithm (OPREC). The vendor ships a wrapper library
 * (liboprec_audrec_wrapper.so) that loads tuning data from
 * /odm/etc/oprec_audrec_params and runs on the application processor.
 *
 * This is the counterpart of Lvacfs and the only microphone calibration path
 * on devices whose resourcemanager.xml advertises record_use_ap_oprec_audrec.
 */
struct oprec_wrapper_ops {
    void* lib_handle;
    int (*create_instance)(void**, uint32_t, uint64_t, uint32_t, uint64_t, uint32_t, uint32_t);
    int (*destroy_instance)(void**);
    int (*process)(void**, void*, size_t, void*, size_t, void*);
    int (*update_zoom_info)(float, void**, int16_t);
    int (*update_angle_info)(void**, int16_t);
    void (*set_params_file_path)(const char*);
    int (*set_profile)(void**, uint32_t);
    int (*set_audio_direction)(void**, uint32_t);
    int (*set_device_orientation)(void**, uint32_t);
    void (*get_versions)(char*);
    int (*get_buffer_size)(uint64_t, uint32_t, uint32_t, uint32_t, size_t*);
    int (*update_input_config)(void*, void*);
    int (*input_config_check)(uint64_t, uint16_t);
    int (*module_dynamic_control)(void**, uint16_t, uint32_t);
    int (*set_runtime_para)(void**, int, uint32_t*);
    int (*get_runtime_para)(void**, int, char*, uint32_t);
};

enum {
    OPREC_SCENE_DEFAULT = 0,
    OPREC_SCENE_INTERVIEW = 1,
    OPREC_SCENE_CONFERENCE = 2,
    OPREC_SCENE_APP1 = 7,
};

class Oprec {
  public:
    static Oprec& getInstance() {
        static Oprec instance;
        return instance;
    }

    static bool isOprecEnabled();

    void init();
    void deinit();

    void startInputStream(StreamInPrimary* in);
    void processInputStream(StreamInPrimary* in, void* buffer, size_t bytes);
    void stopInputStream(StreamInPrimary* in);

    /*
     * Display orientation, 0..3 (portrait, landscape, inverted portrait,
     * inverted landscape). Applied to every active OPREC instance and used
     * for streams started later.
     */
    void setDeviceOrientation(int orientation);

    /*
     * Ported from the stock OplusRecord::getProfile(). Returns the OPREC
     * profile id, or -1 when the device/source combination is not handled by
     * the algorithm.
     */
    static int32_t getProfile(audio_devices_t device, audio_source_t source, int32_t channels,
                              int32_t scene, int32_t effect_type, uint32_t sample_rate);

    /* Ported from the stock OplusOprecAudrec::algoInit() record type selection. */
    static uint32_t getRecordType(pal_stream_type_t type, audio_source_t source, int32_t channels);

    /*
     * Maps an Android PCM format to the format id expected by the wrapper.
     * Returns -1 for formats the algorithm does not accept. Mirrors the
     * stock pcm_format_from_audio_format() mapping.
     */
    static int32_t getFormatId(audio_format_t format);

    const struct oprec_wrapper_ops* getWrapperOps() const { return wrapper_ops_.get(); }

  private:
    Oprec() : wrapper_ops_(nullptr), params_file_path_(nullptr) {}

    Oprec(const Oprec&) = delete;
    Oprec& operator=(const Oprec&) = delete;

    std::unique_ptr<struct oprec_wrapper_ops> wrapper_ops_;
    const char* params_file_path_;

    std::mutex streams_lock_;
    std::set<StreamInPrimary*> streams_;
    int device_orientation_ = 0;
};
