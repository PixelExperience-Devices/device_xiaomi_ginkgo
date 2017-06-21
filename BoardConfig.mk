#
# Copyright (C) 2020-21 Raphielscape LLC. and Haruka LLC.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

DEVICE_PATH := device/xiaomi/ginkgo

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := generic

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := generic

BUILD_BROKEN_DUP_RULES := true

# Bootloader
TARGET_BOOTLOADER_BOARD_NAME := trinket
TARGET_NO_BOOTLOADER := true

# Kernel
BOARD_KERNEL_CMDLINE := console=ttyMSM0,115200n8 androidboot.hardware=qcom
BOARD_KERNEL_CMDLINE += androidboot.console=ttyMSM0
BOARD_KERNEL_CMDLINE += androidboot.memcg=1
BOARD_KERNEL_CMDLINE += lpm_levels.sleep_disabled=1
BOARD_KERNEL_CMDLINE += video=vfb:640x400,bpp=32,memsize=3072000
BOARD_KERNEL_CMDLINE += msm_rtb.filter=0x237
BOARD_KERNEL_CMDLINE += service_locator.enable=1
BOARD_KERNEL_CMDLINE += swiotlb=1 earlycon=msm_geni_serial,0x4a90000
BOARD_KERNEL_CMDLINE += loop.max_part=7
BOARD_KERNEL_CMDLINE += cgroup.memory=nokmem,nosocket
BOARD_KERNEL_CMDLINE += androidboot.init_fatal_reboot_target=recovery
BOARD_KERNEL_CMDLINE += kpti=off
BOARD_KERNEL_CMDLINE += androidboot.selinux=permissive

BOARD_KERNEL_BASE := 0x00000000
BOARD_KERNEL_PAGESIZE :=  4096
BOARD_KERNEL_IMAGE_NAME := Image.gz-dtb
BOARD_KERNEL_OFFSET := 0x00008000
BOARD_KERNEL_TAGS_OFFSET := 0x00000100

BOARD_RAMDISK_OFFSET := 0x01000000

# DTBO partition definitions
BOARD_DTBOIMG_PARTITION_SIZE := 25165824
BOARD_PREBUILT_DTBOIMAGE := device/xiaomi/ginkgo-kernel/dtbo.img

# Audio
AUDIO_FEATURE_ENABLED_COMPRESS_VOIP := false
AUDIO_FEATURE_ENABLED_EXTENDED_COMPRESS_FORMAT := true
AUDIO_FEATURE_ENABLED_FM_POWER_OPT := true

TARGET_PROVIDES_AUDIO_EXTNS := true

# Display
TARGET_SCREEN_DENSITY := 440
TARGET_USES_DRM_PP := true
TARGET_USES_HWC2 := true
TARGET_USES_ION := true
TARGET_USES_COLOR_METADATA := true

# DRM
TARGET_ENABLE_MEDIADRM_64 := true

# Filesystem
TARGET_FS_CONFIG_GEN := $(DEVICE_PATH)/config.fs

# HIDL
DEVICE_FRAMEWORK_MANIFEST_FILE := $(DEVICE_PATH)/framework_manifest.xml
DEVICE_MANIFEST_FILE := $(DEVICE_PATH)/manifest.xml
DEVICE_MATRIX_FILE := $(DEVICE_PATH)/compatibility_matrix.xml

# Partitions - SAR/Metadata
BOARD_BUILD_SYSTEM_ROOT_IMAGE := true
BOARD_USES_METADATA_PARTITION := true

# Partitions - Boot
BOARD_BOOTIMAGE_PARTITION_SIZE := 0x04000000
BOARD_FLASH_BLOCK_SIZE := 262144 # (BOARD_KERNEL_PAGESIZE * 64)

# Partitions - Cache
BOARD_CACHEIMAGE_PARTITION_SIZE := 402653184
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4

# Partitions - Recovery
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 67108864

TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/rootdir/etc/fstab.qcom
TARGET_RECOVERY_PIXEL_FORMAT := "RGBX_8888"
TARGET_RECOVERY_UI_MARGIN_HEIGHT := 80

# Partitions - System
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 4831838208

# Partitions - Userdata
BOARD_USERDATAIMAGE_PARTITION_SIZE := 52554612224

TARGET_USERIMAGES_SPARSE_EXT_DISABLED := false
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true

# Partitions - Vendor
BOARD_VENDORIMAGE_PARTITION_SIZE := 1610612736
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4

TARGET_COPY_OUT_VENDOR := vendor

BOARD_SUPPRESS_SECURE_ERASE := true

# Platform
TARGET_BOARD_PLATFORM := trinket
TRINKET := trinket

# QCOM hardware
BOARD_USES_QCOM_HARDWARE := true

# Releasetools
TARGET_RELEASETOOLS_EXTENSIONS := device/xiaomi/ginkgo-firmware

# RIL
ENABLE_VENDOR_RIL_SERVICE := true

# Sepolicy - Common
include device/qcom/sepolicy_vndr/SEPolicy.mk

# Sepolicy - Ginkgo
include device/xiaomi/ginkgo-sepolicy/ginkgo-sepolicy.mk

# Treble
BOARD_VNDK_VERSION := current

# Inherit from the proprietary version
include vendor/xiaomi/ginkgo/BoardConfigVendor.mk
