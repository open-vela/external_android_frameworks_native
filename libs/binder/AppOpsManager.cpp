/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <mutex>
#include <binder/AppOpsManager.h>
#include <binder/Binder.h>
#include <binder/IServiceManager.h>

#include <utils/SystemClock.h>

#include <sys/types.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AppOpsManager"

namespace android {

namespace {

#if defined(__BRILLO__)
// Because Brillo has no application model, security policy is managed
// statically (at build time) with SELinux controls.
// As a consequence, it also never runs the AppOpsManager service.
const int APP_OPS_MANAGER_UNAVAILABLE_MODE = AppOpsManager::MODE_ALLOWED;
#else
const int APP_OPS_MANAGER_UNAVAILABLE_MODE = AppOpsManager::MODE_IGNORED;
#endif  // defined(__BRILLO__)

}  // namespace

static const sp<IBinder>& getClientId() {
    static pthread_mutex_t gClientIdMutex = PTHREAD_MUTEX_INITIALIZER;
    static sp<IBinder> gClientId;

    pthread_mutex_lock(&gClientIdMutex);
    if (gClientId == nullptr) {
        gClientId = new BBinder();
    }
    pthread_mutex_unlock(&gClientIdMutex);
    return gClientId;
}

thread_local uint64_t notedAppOpsInThisBinderTransaction[2];
thread_local int32_t uidOfThisBinderTransaction = -1;

// Whether an appop should be collected: 0 == not initialized, 1 == don't note, 2 == note
uint8_t appOpsToNote[AppOpsManager::_NUM_OP] = {0};

AppOpsManager::AppOpsManager()
{
}

#if defined(__BRILLO__)
// There is no AppOpsService on Brillo
sp<IAppOpsService> AppOpsManager::getService() { return NULL; }
#else
sp<IAppOpsService> AppOpsManager::getService()
{
    static String16 _appops("appops");

    std::lock_guard<Mutex> scoped_lock(mLock);
    int64_t startTime = 0;
    sp<IAppOpsService> service = mService;
    while (service == nullptr || !IInterface::asBinder(service)->isBinderAlive()) {
        sp<IBinder> binder = defaultServiceManager()->checkService(_appops);
        if (binder == nullptr) {
            // Wait for the app ops service to come back...
            if (startTime == 0) {
                startTime = uptimeMillis();
                ALOGI("Waiting for app ops service");
            } else if ((uptimeMillis()-startTime) > 10000) {
                ALOGW("Waiting too long for app ops service, giving up");
                service = nullptr;
                break;
            }
            sleep(1);
        } else {
            service = interface_cast<IAppOpsService>(binder);
            mService = service;
        }
    }
    return service;
}
#endif  // defined(__BRILLO__)

int32_t AppOpsManager::checkOp(int32_t op, int32_t uid, const String16& callingPackage)
{
    sp<IAppOpsService> service = getService();
    return service != nullptr
            ? service->checkOperation(op, uid, callingPackage)
            : APP_OPS_MANAGER_UNAVAILABLE_MODE;
}

int32_t AppOpsManager::checkAudioOpNoThrow(int32_t op, int32_t usage, int32_t uid,
        const String16& callingPackage) {
    sp<IAppOpsService> service = getService();
    return service != nullptr
           ? service->checkAudioOperation(op, usage, uid, callingPackage)
           : APP_OPS_MANAGER_UNAVAILABLE_MODE;
}

int32_t AppOpsManager::noteOp(int32_t op, int32_t uid, const String16& callingPackage) {
    return noteOp(op, uid, callingPackage, {},
            String16("Legacy AppOpsManager.noteOp call"));
}

int32_t AppOpsManager::noteOp(int32_t op, int32_t uid, const String16& callingPackage,
        const std::optional<String16>& featureId, const String16& message) {
    sp<IAppOpsService> service = getService();
    int32_t mode = service != nullptr
            ? service->noteOperation(op, uid, callingPackage, featureId, shouldCollectNotes(op),
                    message)
            : APP_OPS_MANAGER_UNAVAILABLE_MODE;

    return mode;
}

int32_t AppOpsManager::startOpNoThrow(int32_t op, int32_t uid, const String16& callingPackage,
        bool startIfModeDefault) {
    return startOpNoThrow(op, uid, callingPackage, startIfModeDefault, {},
            String16("Legacy AppOpsManager.startOpNoThrow call"));
}

int32_t AppOpsManager::startOpNoThrow(int32_t op, int32_t uid, const String16& callingPackage,
        bool startIfModeDefault, const std::optional<String16>& featureId,
        const String16& message) {
    sp<IAppOpsService> service = getService();
    int32_t mode = service != nullptr
            ? service->startOperation(getClientId(), op, uid, callingPackage,
                    featureId, startIfModeDefault, shouldCollectNotes(op), message)
            : APP_OPS_MANAGER_UNAVAILABLE_MODE;

    return mode;
}

void AppOpsManager::finishOp(int32_t op, int32_t uid, const String16& callingPackage) {
    finishOp(op, uid, callingPackage, {});
}

void AppOpsManager::finishOp(int32_t op, int32_t uid, const String16& callingPackage,
        const std::optional<String16>& callingFeatureId) {
    sp<IAppOpsService> service = getService();
    if (service != nullptr) {
        service->finishOperation(getClientId(), op, uid, callingPackage, callingFeatureId);
    }
}

void AppOpsManager::startWatchingMode(int32_t op, const String16& packageName,
        const sp<IAppOpsCallback>& callback) {
    sp<IAppOpsService> service = getService();
    if (service != nullptr) {
        service->startWatchingMode(op, packageName, callback);
    }
}

void AppOpsManager::stopWatchingMode(const sp<IAppOpsCallback>& callback) {
    sp<IAppOpsService> service = getService();
    if (service != nullptr) {
        service->stopWatchingMode(callback);
    }
}

int32_t AppOpsManager::permissionToOpCode(const String16& permission) {
    sp<IAppOpsService> service = getService();
    if (service != nullptr) {
        return service->permissionToOpCode(permission);
    }
    return -1;
}

void AppOpsManager::setCameraAudioRestriction(int32_t mode) {
    sp<IAppOpsService> service = getService();
    if (service != nullptr) {
        service->setCameraAudioRestriction(mode);
    }
}

// check it the appops needs to be collected and cache result
bool AppOpsManager::shouldCollectNotes(int32_t opcode) {
    if (appOpsToNote[opcode] == 0) {
        if (getService()->shouldCollectNotes(opcode)) {
            appOpsToNote[opcode] = 2;
        } else {
            appOpsToNote[opcode] = 1;
        }
    }

    return appOpsToNote[opcode] == 2;
}

} // namespace android
