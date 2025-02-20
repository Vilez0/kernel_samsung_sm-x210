#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2019, The Linux Foundation. All rights reserved.

SCRIPT_DIR=$(readlink -f $(dirname $0)/)
cd ${SCRIPT_DIR}
cd ../../
KERN_SRC=`pwd`

: ${ARCH:=arm64}
: ${CROSS_COMPILE:=aarch64-linux-gnu-}
: ${CLANG_TRIPLE:=aarch64-linux-gnu-}
: ${REAL_CC:=clang}
: ${HOSTCC:=gcc}
: ${HOSTLD:=ld}
: ${HOSTAR:=ar}
: ${KERN_OUT:=}

CONFIGS_DIR=${KERN_SRC}/arch/${ARCH}/configs/vendor

PLATFORM_NAME=$1

BASE_DEFCONFIG=${KERN_SRC}/arch/${ARCH}/configs/${2:-gki_defconfig}

# Fragements that are available for the platform
QCOM_GKI_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_GKI.config
QCOM_QGKI_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_QGKI.config
QCOM_DEBUG_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_debug.config

# For user variant build merge debugfs.config fragment.
if [ ${TARGET_BUILD_VARIANT} == "user" ]; then
	QCOM_DEBUG_FS_FRAG=`ls ${CONFIGS_DIR}/debugfs.config 2> /dev/null`
else
	QCOM_DEBUG_FS_FRAG=" "
fi

SEC_COMMON_FRAG=${CONFIGS_DIR}/wingtech_sec_defconfig
SEC_GKI_COMMON_FRAG=${CONFIGS_DIR}/wingtech_sec_GKI_defconfig
SEC_ENG_FRAG=${CONFIGS_DIR}/wingtech_sec_eng_defconfig
SEC_USERDEBUG_FRAG=${CONFIGS_DIR}/wingtech_sec_userdebug_defconfig

echo "Samsung kernel envsetup $SEC_COMMON_FRAG"
echo "Samsung kernel envsetup $SEC_GKI_COMMON_FRAG"
echo "Samsung kernel envsetup $SEC_ENG_FRAG"
echo "Samsung kernel envsetup $SEC_USERDEBUG_FRAG"

# SS Kbuild : Skip below cmd at SS Kbuild.
if [[ -z "${XBUILD_KERNEL_DIR}" ]]; then
  # Consolidate fragment may not be present for all platforms.
  QCOM_CONSOLIDATE_FRAG=`ls ${CONFIGS_DIR}/${PLATFORM_NAME}_consolidate.config 2> /dev/null`
fi

QCOM_GENERIC_PERF_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}.config
QCOM_GENERIC_DEBUG_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}-debug.config

export ARCH CROSS_COMPILE REAL_CC HOSTCC HOSTLD HOSTAR KERN_SRC KERN_OUT \
	CONFIGS_DIR BASE_DEFCONFIG QCOM_GKI_FRAG QCOM_QGKI_FRAG QCOM_DEBUG_FRAG
