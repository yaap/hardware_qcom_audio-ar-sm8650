# Device profile selection

OPREC profile and record type IDs are vendor configuration keys, not Android
audio sources. The same ID can name different tuning on different devices.
Vendor configuration files and libraries do not need to be modified to provide
a device selector.

## Device header

Provide `OprecDeviceProfiles.h` in a vendor `cc_library_headers` module. The
header includes `OprecProfileConfig.h` and defines:

```cpp
namespace oprec {
inline Selection selectProfile(const Capture& capture) {
    // Select a vendor profile and record type using the device's stock HAL
    // behaviour. Return {-1, 0} for captures that should bypass OPREC.
}
}
```

`Capture` contains the Android source and input device, sample rate, channel
count, recording scene, vendor effect type and whether the stream uses a low
latency PAL stream type. Selection rules belong to the device header because
some vendor HALs select by source while others select by rate, channel count or
effect type.

Select the module in the device's `device.mk`:

```make
$(call soong_config_set,qtiaudio,oprec_headers,//device/vendor/device:liboprec_device_headers)
```

The module exports the directory containing `OprecDeviceProfiles.h` and any
generated constants. The HAL's local include directory supplies
`OprecProfileConfig.h`; do not copy it into the device tree.

Without `oprec_headers`, the default selector reproduces the OPLUS mapping that
shipped with the initial OPREC integration. Devices that need a different
mapping must opt in.

## Recording scene and effect type

The HAL parses `local_record_mode` (`interview`, `conference`, `app1`) and
`vendor_audiorecord_effect_type` from the global audio parameters, like the
LVACFS implementation does. A selector can use `capture.scene` and
`capture.effect_type` to reach the scene and three-mic profiles; without those
keys both stay at their defaults.

## Generate constants from vendor files

From the Android source root, run:

```sh
python3 hardware/qcom-caf/sm8650/audio/primary-hal/tools/generate_oprec_profiles.py \
    vendor/vendor/device/proprietary/odm/etc/oprec_audrec_params \
    device/vendor/device/configs/audio/oprec/OprecProfiles.h
```

The output directory must already exist. The generator reads the supplied
parameter file names without changing them. Check the generated header into the
device tree and regenerate it when updating vendor tuning.

Constants use names derived from the parameter file basenames, with the IDs
taken from the `ID<n>` part of the name. The `.bin` and `.txt` pair for one
profile produces a single constant.

The generator emits constants only. It cannot infer the stock HAL's selection
rules from filenames. Review the device selector against the stock caller,
including route, rate, channel count, scene and effect type changes.

Run the generator tests with:

```sh
python3 hardware/qcom-caf/sm8650/audio/primary-hal/tools/test_generate_oprec_profiles.py
```
