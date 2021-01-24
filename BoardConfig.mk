#
# Copyright (C) 2018-2019 The LineageOS Project
# Copyright (C) 2020 Paranoid Android
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from trinket-common
-include device/xiaomi/trinket-common/BoardConfigCommon.mk

DEVICE_PATH := device/xiaomi/ginkgo

# Audio
TARGET_PROVIDES_AUDIO_EXTNS := true
AUDIO_FEATURE_ENABLED_INCALL_MUSIC := true

# Assert
TARGET_OTA_ASSERT_DEVICE := ginkgo,willow

# Display
TARGET_SCREEN_DENSITY := 440

# DT2W
TARGET_TAP_TO_WAKE_NODE := "/sys/touchpanel/double_tap"

# HIDL
DEVICE_MANIFEST_FILE += $(DEVICE_PATH)/configs/hidl/manifest.xml
ODM_MANIFEST_WILLOW_FILES := $(DEVICE_PATH)/configs/hidl/manifest_willow.xml
ODM_MANIFEST_SKUS += willow

# Init
TARGET_INIT_VENDOR_LIB := //$(DEVICE_PATH):libinit_ginkgo
TARGET_RECOVERY_DEVICE_MODULES := libinit_ginkgo

# Kernel
TARGET_KERNEL_SOURCE := kernel/xiaomi/ginkgo
TARGET_KERNEL_CONFIG := vendor/ginkgo-perf_defconfig
BOARD_VENDOR_KERNEL_MODULES += \
    $(DEVICE_PATH)/kernel-modules/rmnet_perf.ko \
    $(DEVICE_PATH)/kernel-modules/rmnet_shs.ko

# NFC
TARGET_USES_NQ_NFC := true

# Partitions
BOARD_CACHEIMAGE_PARTITION_SIZE := 402653184
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_DTBOIMG_PARTITION_SIZE := 25165824
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 67108864
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 4831838208
BOARD_USERDATAIMAGE_PARTITION_SIZE := 52554612224
BOARD_VENDORIMAGE_PARTITION_SIZE := 1610612736

# Power
BOARD_POWER_CUSTOM_BOARD_LIB := libpower_ginkgo
TARGET_POWERHAL_MODE_EXT := $(DEVICE_PATH)/power/power-mode.cpp

# Properties
TARGET_SYSTEM_PROP += $(DEVICE_PATH)/system.prop
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# Recovery
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/rootdir/etc/fstab.qcom

# Releasetools
TARGET_RELEASETOOLS_EXTENSIONS := $(DEVICE_PATH)

# Sepolicy
BOARD_PLAT_PUBLIC_SEPOLICY_DIR += $(DEVICE_PATH)/sepolicy/public
BOARD_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/vendor

# Inherit from the proprietary version
-include vendor/xiaomi/ginkgo/BoardConfigVendor.mk
