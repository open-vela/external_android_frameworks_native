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

#define LOG_TAG "AppOpsService"

#include <binder/IAppOpsService.h>

#include <utils/Debug.h>
#include <utils/Log.h>
#include <binder/Parcel.h>
#include <utils/String8.h>

#include <private/binder/Static.h>

namespace android {

// ----------------------------------------------------------------------

class BpAppOpsService : public BpInterface<IAppOpsService>
{
public:
    BpAppOpsService(const sp<IBinder>& impl)
        : BpInterface<IAppOpsService>(impl)
    {
    }

    virtual int32_t checkOperation(int32_t code, int32_t uid, const String16& packageName) {
        Parcel data, reply;
        data.writeInterfaceToken(IAppOpsService::getInterfaceDescriptor());
        data.writeInt32(code);
        data.writeInt32(uid);
        data.writeString16(packageName);
        remote()->transact(CHECK_OPERATION_TRANSACTION, data, &reply);
        // fail on exception
        if (reply.readExceptionCode() != 0) return MODE_ERRORED;
        return reply.readInt32();
    }

    virtual int32_t noteOperation(int32_t code, int32_t uid, const String16& packageName) {
        Parcel data, reply;
        data.writeInterfaceToken(IAppOpsService::getInterfaceDescriptor());
        data.writeInt32(code);
        data.writeInt32(uid);
        data.writeString16(packageName);
        remote()->transact(NOTE_OPERATION_TRANSACTION, data, &reply);
        // fail on exception
        if (reply.readExceptionCode() != 0) return MODE_ERRORED;
        return reply.readInt32();
    }

    virtual int32_t startOperation(int32_t code, int32_t uid, const String16& packageName) {
        Parcel data, reply;
        data.writeInterfaceToken(IAppOpsService::getInterfaceDescriptor());
        data.writeInt32(code);
        data.writeInt32(uid);
        data.writeString16(packageName);
        remote()->transact(START_OPERATION_TRANSACTION, data, &reply);
        // fail on exception
        if (reply.readExceptionCode() != 0) return MODE_ERRORED;
        return reply.readInt32();
    }

    virtual void finishOperation(int32_t code, int32_t uid, const String16& packageName) {
        Parcel data, reply;
        data.writeInterfaceToken(IAppOpsService::getInterfaceDescriptor());
        data.writeInt32(code);
        data.writeInt32(uid);
        data.writeString16(packageName);
        remote()->transact(FINISH_OPERATION_TRANSACTION, data, &reply);
    }

    virtual void startWatchingMode(int32_t op, const String16& packageName,
            const sp<IAppOpsCallback>& callback) {
        Parcel data, reply;
        data.writeInterfaceToken(IAppOpsService::getInterfaceDescriptor());
        data.writeInt32(op);
        data.writeString16(packageName);
        data.writeStrongBinder(callback->asBinder());
        remote()->transact(START_WATCHING_MODE_TRANSACTION, data, &reply);
    }

    virtual void stopWatchingMode(const sp<IAppOpsCallback>& callback) {
        Parcel data, reply;
        data.writeInterfaceToken(IAppOpsService::getInterfaceDescriptor());
        data.writeStrongBinder(callback->asBinder());
        remote()->transact(STOP_WATCHING_MODE_TRANSACTION, data, &reply);
    }
};

IMPLEMENT_META_INTERFACE(AppOpsService, "com.android.internal.app.IAppOpsService");

// ----------------------------------------------------------------------

status_t BnAppOpsService::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    //printf("AppOpsService received: "); data.print();
    switch(code) {
        case CHECK_OPERATION_TRANSACTION: {
            CHECK_INTERFACE(IAppOpsService, data, reply);
            int32_t code = data.readInt32();
            int32_t uid = data.readInt32();
            String16 packageName = data.readString16();
            int32_t res = checkOperation(code, uid, packageName);
            reply->writeNoException();
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case NOTE_OPERATION_TRANSACTION: {
            CHECK_INTERFACE(IAppOpsService, data, reply);
            int32_t code = data.readInt32();
            int32_t uid = data.readInt32();
            String16 packageName = data.readString16();
            int32_t res = noteOperation(code, uid, packageName);
            reply->writeNoException();
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case START_OPERATION_TRANSACTION: {
            CHECK_INTERFACE(IAppOpsService, data, reply);
            int32_t code = data.readInt32();
            int32_t uid = data.readInt32();
            String16 packageName = data.readString16();
            int32_t res = startOperation(code, uid, packageName);
            reply->writeNoException();
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case FINISH_OPERATION_TRANSACTION: {
            CHECK_INTERFACE(IAppOpsService, data, reply);
            int32_t code = data.readInt32();
            int32_t uid = data.readInt32();
            String16 packageName = data.readString16();
            finishOperation(code, uid, packageName);
            reply->writeNoException();
            return NO_ERROR;
        } break;
        case START_WATCHING_MODE_TRANSACTION: {
            CHECK_INTERFACE(IAppOpsService, data, reply);
            int32_t op = data.readInt32();
            String16 packageName = data.readString16();
            sp<IAppOpsCallback> callback = interface_cast<IAppOpsCallback>(data.readStrongBinder());
            startWatchingMode(op, packageName, callback);
            reply->writeNoException();
            return NO_ERROR;
        } break;
        case STOP_WATCHING_MODE_TRANSACTION: {
            CHECK_INTERFACE(IAppOpsService, data, reply);
            sp<IAppOpsCallback> callback = interface_cast<IAppOpsCallback>(data.readStrongBinder());
            stopWatchingMode(callback);
            reply->writeNoException();
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

}; // namespace android
