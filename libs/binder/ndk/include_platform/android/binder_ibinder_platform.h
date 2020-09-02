/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <android/binder_ibinder.h>

#if !defined(__ANDROID_APEX__) && !defined(__ANDROID_VNDK__)
#include <binder/IBinder.h>
#endif

__BEGIN_DECLS

/**
 * Makes calls to AIBinder_getCallingSid work if the kernel supports it. This
 * must be called on a local binder server before it is sent out to any othe
 * process. If this is a remote binder, it will abort. If the kernel doesn't
 * support this feature, you'll always get null from AIBinder_getCallingSid.
 *
 * \param binder local server binder to request security contexts on
 */
__attribute__((weak)) void AIBinder_setRequestingSid(AIBinder* binder, bool requestingSid)
        __INTRODUCED_IN(31);

/**
 * Returns the selinux context of the callee.
 *
 * In order for this to work, the following conditions must be met:
 * - The kernel must be new enough to support this feature.
 * - The server must have called AIBinder_setRequestingSid.
 * - The callee must be a remote process.
 *
 * \return security context or null if unavailable. The lifetime of this context
 * is the lifetime of the transaction.
 */
__attribute__((weak, warn_unused_result)) const char* AIBinder_getCallingSid() __INTRODUCED_IN(31);

__END_DECLS

#if !defined(__ANDROID_APEX__) && !defined(__ANDROID_VNDK__)

/**
 * Get libbinder version of binder from AIBinder.
 *
 * WARNING: function calls to a local object on the other side of this function
 * will parcel. When converting between binders, keep in mind it is not as
 * efficient as a direct function call.
 *
 * \param binder binder with ownership retained by the client
 * \return platform binder object
 */
android::sp<android::IBinder> AIBinder_toPlatformBinder(AIBinder* binder);

/**
 * Get libbinder_ndk version of binder from platform binder.
 *
 * WARNING: function calls to a local object on the other side of this function
 * will parcel. When converting between binders, keep in mind it is not as
 * efficient as a direct function call.
 *
 * \param binder platform binder which may be from anywhere (doesn't have to be
 * created with libbinder_ndK)
 * \return binder with one reference count of ownership given to the client. See
 * AIBinder_decStrong
 */
AIBinder* AIBinder_fromPlatformBinder(const android::sp<android::IBinder>& binder);

#endif
