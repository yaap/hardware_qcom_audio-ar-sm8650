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

#include "OprecProfileConfig.h"
#include "PalApi.h"
#include "PalDefs.h"

class StreamInPrimary;
struct str_parms;

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

class Oprec {
  public:
    static Oprec& getInstance() {
        static Oprec instance;
        return instance;
    }

    static bool isOprecEnabled();

    void init();
    void deinit();

    /* Recording scene and effect type, mirroring the LVACFS implementation. */
    void setParameters(struct str_parms* parms);

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
     * Builds the capture descriptor for a stream and asks the device profile
     * header for the vendor profile and record type. The default header
     * ships the stock OPLUS mapping.
     */
    oprec::Selection selectCapture(StreamInPrimary* in) const;

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

    /* Recording scene and effect type from the OPLUS recording parameters. */
    mutable std::mutex lock_;
    oprec::Scene scene_ = oprec::Scene::DEFAULT;
    int effect_type_ = -1;

    std::mutex streams_lock_;
    std::set<StreamInPrimary*> streams_;
    int device_orientation_ = 0;
};
