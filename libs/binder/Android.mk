# Copyright (C) 2009 The Android Open Source Project
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

# we have the common sources, plus some device-specific stuff
sources := \
    AppOpsManager.cpp \
    Binder.cpp \
    BpBinder.cpp \
    BufferedTextOutput.cpp \
    Debug.cpp \
    IAppOpsCallback.cpp \
    IAppOpsService.cpp \
    IBatteryStats.cpp \
    IInterface.cpp \
    IMemory.cpp \
    IPCThreadState.cpp \
    IPermissionController.cpp \
    IProcessInfoService.cpp \
    IServiceManager.cpp \
    MemoryBase.cpp \
    MemoryDealer.cpp \
    MemoryHeapBase.cpp \
    Parcel.cpp \
    PermissionCache.cpp \
    PersistableBundle.cpp \
    ProcessInfoService.cpp \
    ProcessState.cpp \
    Static.cpp \
    Status.cpp \
    TextOutput.cpp \

LOCAL_PATH:= $(call my-dir)

binder_cflags := -Wall -Wextra -Werror
ifneq ($(TARGET_USES_64_BIT_BINDER),true)
ifneq ($(TARGET_IS_64_BIT),true)
binder_cflags += -DBINDER_IPC_32BIT=1
endif
endif

include $(CLEAR_VARS)
LOCAL_MODULE := libbinder
LOCAL_SHARED_LIBRARIES := libbase liblog libcutils libutils
LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := libbase libutils

LOCAL_SANITIZE := integer
LOCAL_SRC_FILES := $(sources)
LOCAL_CFLAGS := $(binder_cflags)
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libbinder
LOCAL_STATIC_LIBRARIES := libbase libutils
LOCAL_EXPORT_STATIC_LIBRARY_HEADERS := libbase libutils
LOCAL_SRC_FILES := $(sources)
LOCAL_CFLAGS := $(binder_cflags)
include $(BUILD_STATIC_LIBRARY)
