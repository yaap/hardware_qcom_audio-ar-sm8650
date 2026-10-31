/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AHAL: Oprec"

#include <AudioStream.h>
#include <Oprec.h>
#include <dlfcn.h>
#include <log/log.h>
#include <unistd.h>

#ifdef __LP64__
#define VENDOR_LIBS "/vendor/lib64/"
#define ODM_LIBS "/odm/lib64/"
#else
#define VENDOR_LIBS "/vendor/lib/"
#define ODM_LIBS "/odm/lib/"
#endif
#define OPREC_WRAPPER_LIB_NAME "liboprec_audrec_wrapper.so"
#define VENDOR_LIB_PATH VENDOR_LIBS OPREC_WRAPPER_LIB_NAME
#define ODM_LIB_PATH ODM_LIBS OPREC_WRAPPER_LIB_NAME

#define ODM_PARAMS_DIR_PATH "/odm/etc/oprec_audrec_params"
#define VENDOR_PARAMS_DIR_PATH "/vendor/etc/oprec_audrec_params"

#define LOAD_SYMBOL(handle, symbol_ptr, symbol_name)                           \
    do {                                                                       \
        (symbol_ptr) = (decltype(symbol_ptr))dlsym((handle), (symbol_name));   \
        if (!(symbol_ptr)) {                                                   \
            ALOGE("Failed to load symbol '%s': %s", (symbol_name), dlerror()); \
            deinit();                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

bool Oprec::isOprecEnabled() {
    bool isEnabled = false;
    bool* payload_oprec = &isEnabled;
    size_t param_size = 0;

    int ret = pal_get_param(PAL_PARAM_ID_OPREC_AUDREC, (void**)&payload_oprec, &param_size,
                            nullptr);

    return (!ret && isEnabled);
}

void Oprec::init() {
    if (access(ODM_PARAMS_DIR_PATH, F_OK) == 0) {
        params_file_path_ = ODM_PARAMS_DIR_PATH;
    } else if (access(VENDOR_PARAMS_DIR_PATH, F_OK) == 0) {
        params_file_path_ = VENDOR_PARAMS_DIR_PATH;
    } else {
        ALOGE("No params directory found");
        return;
    }

    wrapper_ops_ = std::make_unique<struct oprec_wrapper_ops>();
    if (!wrapper_ops_) {
        ALOGE("Failed to allocate oprec_wrapper_ops");
        return;
    }

    wrapper_ops_->lib_handle = dlopen(ODM_LIB_PATH, RTLD_NOW);
    if (!wrapper_ops_->lib_handle) {
        wrapper_ops_->lib_handle = dlopen(VENDOR_LIB_PATH, RTLD_NOW);
        if (!wrapper_ops_->lib_handle) {
            ALOGE("dlopen failed for %s and %s: %s", ODM_LIB_PATH, VENDOR_LIB_PATH, dlerror());
            return;
        }
    }

    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->create_instance,
                "oprec_audrec_wrapper_CreateLibraryInstance");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->destroy_instance,
                "oprec_audrec_wrapper_DestroyLibraryInstance");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->process, "oprec_audrec_wrapper_Process");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_params_file_path,
                "oprec_audrec_SetParamsFilePath");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_profile,
                "oprec_audrec_wrapper_SetProfile");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->update_zoom_info,
                "oprec_audrec_wrapper_UpdateZoomInfo");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->update_angle_info,
                "oprec_audrec_wrapper_UpdateAngleInfo");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_audio_direction,
                "oprec_audrec_wrapper_SetAudioDirection");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_device_orientation,
                "oprec_audrec_wrapper_SetDeviceOrientation");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->get_versions,
                "oprec_audrec_wrapper_GetVersions");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->get_buffer_size,
                "oprec_audrec_wrapper_GetBufferSize");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->update_input_config,
                "oprec_audrec_wrapper_UpdateInputConfig");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->input_config_check,
                "oprec_audrec_wrapper_InputConfigCheck");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->module_dynamic_control,
                "oprec_audrec_wrapper_module_dynamic_control");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_runtime_para,
                "oprec_audrec_wrapper_set_runtimePara");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->get_runtime_para,
                "oprec_audrec_wrapper_get_runtimePara");

    char version[64] = {0};
    wrapper_ops_->get_versions(version);
    ALOGI("OPREC %s initialized successfully", version);
}

void Oprec::deinit() {
    if (wrapper_ops_ && wrapper_ops_->lib_handle) {
        dlclose(wrapper_ops_->lib_handle);
    }
    wrapper_ops_.reset();
    ALOGI("OPREC deinitialized");
}

void Oprec::startInputStream(StreamInPrimary* in) {
    if (!wrapper_ops_ || !params_file_path_) {
        ALOGE("OPREC is not initialized");
        return;
    }

    in->oprec_handle = (void**)calloc(1, sizeof(void*));
    if (!in->oprec_handle) {
        ALOGE("Failed to allocate oprec_handle");
        return;
    }

    wrapper_ops_->set_params_file_path(params_file_path_);

    int channel_count = audio_channel_count_from_in_mask(in->config_.channel_mask);
    if (channel_count <= 0 || channel_count > 4) {
        ALOGE("Invalid channel count %d", channel_count);
        free(in->oprec_handle);
        in->oprec_handle = nullptr;
        return;
    }

    int32_t format_id = getFormatId(in->config_.format);
    if (format_id < 0) {
        ALOGE("Unsupported format %#x", in->config_.format);
        free(in->oprec_handle);
        in->oprec_handle = nullptr;
        return;
    }

    /* The wrapper takes (format id << 32) | sample rate. */
    uint64_t rate_format = (((uint64_t)format_id) << 32) | in->config_.sample_rate;
    int ret = wrapper_ops_->create_instance(in->oprec_handle, in->oprec_profile_, rate_format,
                                            channel_count, rate_format, channel_count,
                                            in->oprec_record_type_);
    if (ret != 0) {
        ALOGE("create instance failed: %d", ret);
        stopInputStream(in);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(streams_lock_);
        streams_.insert(in);
    }
    {
        std::lock_guard<std::mutex> lock(in->oprec_lock);
        wrapper_ops_->set_device_orientation(in->oprec_handle, device_orientation_);
    }

    ALOGI("OPREC started: profile=%d record_type=%u rate=%u channels=%d format=%#x id=%d",
          in->oprec_profile_, in->oprec_record_type_, in->config_.sample_rate, channel_count,
          in->config_.format, format_id);
}

void Oprec::processInputStream(StreamInPrimary* in, void* buffer, size_t bytes) {
    if (!wrapper_ops_ || !in->oprec_handle || !*in->oprec_handle) {
        return;
    }

    std::lock_guard<std::mutex> lock(in->oprec_lock);
    uint8_t status_buffer[0x160] = {0};
    int ret = wrapper_ops_->process(in->oprec_handle, buffer, bytes, buffer, bytes,
                                    status_buffer);
    if (ret != 0) {
        ALOGE("process failed: %d", ret);
    }
}

void Oprec::stopInputStream(StreamInPrimary* in) {
    if (!in->oprec_handle) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(streams_lock_);
        streams_.erase(in);
    }

    std::lock_guard<std::mutex> lock(in->oprec_lock);
    if (wrapper_ops_ && *in->oprec_handle) {
        wrapper_ops_->destroy_instance(in->oprec_handle);
    }
    free(in->oprec_handle);
    in->oprec_handle = nullptr;
    ALOGI("OPREC stopped");
}

void Oprec::setDeviceOrientation(int orientation) {
    if (orientation < 0 || orientation > 3) {
        ALOGE("Invalid device orientation %d", orientation);
        return;
    }

    device_orientation_ = orientation;
    if (!wrapper_ops_) {
        return;
    }

    std::lock_guard<std::mutex> lock(streams_lock_);
    for (auto* in : streams_) {
        std::lock_guard<std::mutex> stream_lock(in->oprec_lock);
        if (in->oprec_handle && *in->oprec_handle) {
            wrapper_ops_->set_device_orientation(in->oprec_handle, orientation);
        }
    }
}

int32_t Oprec::getProfile(audio_devices_t device, audio_source_t source, int32_t channels,
                          int32_t scene, int32_t effect_type, uint32_t sample_rate) {
    const bool is_builtin = device == AUDIO_DEVICE_IN_BUILTIN_MIC ||
                            device == AUDIO_DEVICE_IN_BACK_MIC;
    const bool is_wired = device == AUDIO_DEVICE_IN_WIRED_HEADSET;
    const bool is_usb = device == AUDIO_DEVICE_IN_USB_DEVICE ||
                        device == AUDIO_DEVICE_IN_USB_HEADSET;

    /*
     * Three/four channel camcorder capture selects its profile from the vendor
     * effect type, exactly like the stock OplusRecord::getThreeMicProfile().
     */
    if (is_builtin && source == AUDIO_SOURCE_CAMCORDER && channels > 2) {
        static const int32_t kThreeMicProfiles[] = {0, 1, 2, 3, -1, 0, 1};
        if (effect_type < 0 ||
            effect_type >= (int32_t)(sizeof(kThreeMicProfiles) / sizeof(kThreeMicProfiles[0]))) {
            return -1;
        }
        return kThreeMicProfiles[effect_type];
    }

    int32_t profile;
    if (is_builtin) {
        if (scene == OPREC_SCENE_INTERVIEW) return 7;
        if (scene == OPREC_SCENE_CONFERENCE) return 5;
        switch (source) {
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
        if (scene == OPREC_SCENE_INTERVIEW) return 17;
        if (scene == OPREC_SCENE_CONFERENCE) return 15;
        switch (source) {
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
        if (scene == OPREC_SCENE_INTERVIEW) return 17;
        if (scene == OPREC_SCENE_CONFERENCE) return 15;
        switch (source) {
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
        return -1;
    }

    /* Scene 7 (app1) at 16 kHz uses the matching *_APP1 profile. */
    if (scene == OPREC_SCENE_APP1 && sample_rate == 16000) {
        profile++;
    }

    return profile;
}

uint32_t Oprec::getRecordType(pal_stream_type_t type, audio_source_t source, int32_t channels) {
    uint32_t record_type = 0;

    if (type == PAL_STREAM_LOW_LATENCY || type == PAL_STREAM_ULTRA_LOW_LATENCY) {
        record_type = 1;
    }

    /* The stock HAL only uses the camcorder record types for >2 channels. */
    if (source == AUDIO_SOURCE_CAMCORDER && channels > 2) {
        record_type = 3;
    }

    return record_type;
}

int32_t Oprec::getFormatId(audio_format_t format) {
    switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            return 0;
        case AUDIO_FORMAT_PCM_32_BIT:
            return 1;
        case AUDIO_FORMAT_PCM_8_24_BIT:
            return 3;
        case AUDIO_FORMAT_PCM_24_BIT_PACKED:
            return 4;
        default:
            return -1;
    }
}
