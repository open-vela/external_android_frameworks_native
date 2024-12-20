/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <android/binder_ibinder.h>
#include <android/binder_ibinder_platform.h>
#include <android/binder_libbinder.h>
#include "ibinder_internal.h"

#include <android/binder_stability.h>
#include <android/binder_status.h>
#include "parcel_internal.h"
#include "status_internal.h"

#include <android-base/logging.h>
#include <binder/IPCThreadState.h>
#include <binder/IResultReceiver.h>
#include <private/android_filesystem_config.h>

using DeathRecipient = ::android::IBinder::DeathRecipient;

using ::android::IBinder;
using ::android::IResultReceiver;
using ::android::Parcel;
using ::android::sp;
using ::android::status_t;
using ::android::String16;
using ::android::String8;
using ::android::wp;

namespace ABBinderTag {

static const void* kId = "ABBinder";
static void* kValue = static_cast<void*>(new bool{true});
void clean(const void* /*id*/, void* /*obj*/, void* /*cookie*/){/* do nothing */};

static void attach(const sp<IBinder>& binder) {
    // can only attach once
    CHECK_EQ(nullptr, binder->attachObject(kId, kValue, nullptr /*cookie*/, clean));
}
static bool has(const sp<IBinder>& binder) {
    return binder != nullptr && binder->findObject(kId) == kValue;
}

}  // namespace ABBinderTag

namespace ABpBinderTag {

static const void* kId = "ABpBinder";
struct Value {
    wp<ABpBinder> binder;
};
void clean(const void* id, void* obj, void* cookie) {
    CHECK(id == kId) << id << " " << obj << " " << cookie;

    delete static_cast<Value*>(obj);
};

}  // namespace ABpBinderTag

AIBinder::AIBinder(const AIBinder_Class* clazz) : mClazz(clazz) {}
AIBinder::~AIBinder() {}

std::optional<bool> AIBinder::associateClassInternal(const AIBinder_Class* clazz,
                                                     const String16& newDescriptor, bool set) {
    std::lock_guard<std::mutex> lock(mClazzMutex);
    if (mClazz == clazz) return true;

    if (mClazz != nullptr) {
        const String16& currentDescriptor = mClazz->getInterfaceDescriptor();
        if (newDescriptor == currentDescriptor) {
            LOG(ERROR) << __func__ << ": Class descriptors '" << currentDescriptor
                       << "' match during associateClass, but they are different class objects ("
                       << clazz << " vs " << mClazz << "). Class descriptor collision?";
        } else {
            LOG(ERROR) << __func__
                       << ": Class cannot be associated on object which already has a class. "
                          "Trying to associate to '"
                       << newDescriptor << "' but already set to '" << currentDescriptor << "'.";
        }

        // always a failure because we know mClazz != clazz
        return false;
    }

    if (set) {
        // if this is a local object, it's not one known to libbinder_ndk
        mClazz = clazz;
        return true;
    }

    return {};
}

// b/175635923 libcxx causes "implicit-conversion" with a string with invalid char
static std::string SanitizeString(const String16& str) {
    std::string sanitized{String8(str)};
    for (auto& c : sanitized) {
        if (!isprint(c)) {
            c = '?';
        }
    }
    return sanitized;
}

bool AIBinder::associateClass(const AIBinder_Class* clazz) {
    if (clazz == nullptr) return false;

    const String16& newDescriptor = clazz->getInterfaceDescriptor();

    auto result = associateClassInternal(clazz, newDescriptor, false);
    if (result.has_value()) return *result;

    CHECK(asABpBinder() != nullptr);  // ABBinder always has a descriptor

    const String16& descriptor = getBinder()->getInterfaceDescriptor();
    if (descriptor != newDescriptor) {
        if (getBinder()->isBinderAlive()) {
            LOG(ERROR) << __func__ << ": Expecting binder to have class '" << newDescriptor
                       << "' but descriptor is actually '" << SanitizeString(descriptor) << "'.";
        } else {
            // b/155793159
            LOG(ERROR) << __func__ << ": Cannot associate class '" << newDescriptor
                       << "' to dead binder.";
        }
        return false;
    }

    return associateClassInternal(clazz, newDescriptor, true).value();
}

ABBinder::ABBinder(const AIBinder_Class* clazz, void* userData)
    : AIBinder(clazz), BBinder(), mUserData(userData) {
    CHECK(clazz != nullptr);
}
ABBinder::~ABBinder() {
    getClass()->onDestroy(mUserData);
}

const String16& ABBinder::getInterfaceDescriptor() const {
    return getClass()->getInterfaceDescriptor();
}

status_t ABBinder::dump(int fd, const ::android::Vector<String16>& args) {
    AIBinder_onDump onDump = getClass()->onDump;

    if (onDump == nullptr) {
        return STATUS_OK;
    }

    // technically UINT32_MAX would be okay here, but INT32_MAX is expected since this may be
    // null in Java
    if (args.size() > INT32_MAX) {
        LOG(ERROR) << "ABBinder::dump received too many arguments: " << args.size();
        return STATUS_BAD_VALUE;
    }

    std::vector<String8> utf8Args;  // owns memory of utf8s
    utf8Args.reserve(args.size());
    std::vector<const char*> utf8Pointers;  // what can be passed over NDK API
    utf8Pointers.reserve(args.size());

    for (size_t i = 0; i < args.size(); i++) {
        utf8Args.push_back(String8(args[i]));
        utf8Pointers.push_back(utf8Args[i].c_str());
    }

    return onDump(this, fd, utf8Pointers.data(), utf8Pointers.size());
}

status_t ABBinder::onTransact(transaction_code_t code, const Parcel& data, Parcel* reply,
                              binder_flags_t flags) {
    if (isUserCommand(code)) {
        if (getClass()->writeHeader && !data.checkInterface(this)) {
            return STATUS_BAD_TYPE;
        }

        const AParcel in = AParcel::readOnly(this, &data);
        AParcel out = AParcel(this, reply, false /*owns*/);

        binder_status_t status = getClass()->onTransact(this, code, &in, &out);
        return PruneStatusT(status);
    } else if (code == SHELL_COMMAND_TRANSACTION && getClass()->handleShellCommand != nullptr) {
        int in = data.readFileDescriptor();
        int out = data.readFileDescriptor();
        int err = data.readFileDescriptor();

        int argc = data.readInt32();
        std::vector<String8> utf8Args;          // owns memory of utf8s
        std::vector<const char*> utf8Pointers;  // what can be passed over NDK API
        for (int i = 0; i < argc && data.dataAvail() > 0; i++) {
            utf8Args.push_back(String8(data.readString16()));
            utf8Pointers.push_back(utf8Args[i].c_str());
        }

        data.readStrongBinder();  // skip over the IShellCallback
        sp<IResultReceiver> resultReceiver = IResultReceiver::asInterface(data.readStrongBinder());

        // Shell commands should only be callable by ADB.
        uid_t uid = AIBinder_getCallingUid();
        if (uid != AID_ROOT && uid != AID_SHELL) {
            if (resultReceiver != nullptr) {
                resultReceiver->send(-1);
            }
            return STATUS_PERMISSION_DENIED;
        }

        // Check that the file descriptors are valid.
        if (in == STATUS_BAD_TYPE || out == STATUS_BAD_TYPE || err == STATUS_BAD_TYPE) {
            if (resultReceiver != nullptr) {
                resultReceiver->send(-1);
            }
            return STATUS_BAD_VALUE;
        }

        binder_status_t status = getClass()->handleShellCommand(
                this, in, out, err, utf8Pointers.data(), utf8Pointers.size());
        if (resultReceiver != nullptr) {
            resultReceiver->send(status);
        }
        return status;
    } else {
        return BBinder::onTransact(code, data, reply, flags);
    }
}

ABpBinder::ABpBinder(const ::android::sp<::android::IBinder>& binder)
    : AIBinder(nullptr /*clazz*/), BpRefBase(binder) {
    CHECK(binder != nullptr);
}
ABpBinder::~ABpBinder() {}

void ABpBinder::onLastStrongRef(const void* id) {
    // Since ABpBinder is OBJECT_LIFETIME_WEAK, we must remove this weak reference in order for
    // the ABpBinder to be deleted. Even though we have no more references on the ABpBinder
    // (BpRefBase), the remote object may still exist (for instance, if we
    // receive it from another process, before the ABpBinder is attached).

    ABpBinderTag::Value* value =
            static_cast<ABpBinderTag::Value*>(remote()->findObject(ABpBinderTag::kId));
    CHECK_NE(nullptr, value) << "ABpBinder must always be attached";

    remote()->withLock([&]() { value->binder = nullptr; });

    BpRefBase::onLastStrongRef(id);
}

sp<AIBinder> ABpBinder::lookupOrCreateFromBinder(const ::android::sp<::android::IBinder>& binder) {
    if (binder == nullptr) {
        return nullptr;
    }
    if (ABBinderTag::has(binder)) {
        return static_cast<ABBinder*>(binder.get());
    }

    // The following code ensures that for a given binder object (remote or local), if it is not an
    // ABBinder then at most one ABpBinder object exists in a given process representing it.

    auto* value = static_cast<ABpBinderTag::Value*>(binder->findObject(ABpBinderTag::kId));
    if (value == nullptr) {
        value = new ABpBinderTag::Value;
        auto oldValue = static_cast<ABpBinderTag::Value*>(
                binder->attachObject(ABpBinderTag::kId, static_cast<void*>(value),
                                     nullptr /*cookie*/, ABpBinderTag::clean));

        // allocated by another thread
        if (oldValue) {
            delete value;
            value = oldValue;
        }
    }

    sp<ABpBinder> ret;
    binder->withLock([&]() {
        ret = value->binder.promote();
        if (ret == nullptr) {
            ret = sp<ABpBinder>::make(binder);
            value->binder = ret;
        }
    });

    return ret;
}

struct AIBinder_Weak {
    wp<AIBinder> binder;
};
AIBinder_Weak* AIBinder_Weak_new(AIBinder* binder) {
    if (binder == nullptr) {
        return nullptr;
    }

    return new AIBinder_Weak{wp<AIBinder>(binder)};
}
void AIBinder_Weak_delete(AIBinder_Weak* weakBinder) {
    delete weakBinder;
}
AIBinder* AIBinder_Weak_promote(AIBinder_Weak* weakBinder) {
    if (weakBinder == nullptr) {
        return nullptr;
    }

    sp<AIBinder> binder = weakBinder->binder.promote();
    AIBinder_incStrong(binder.get());
    return binder.get();
}

AIBinder_Weak* AIBinder_Weak_clone(const AIBinder_Weak* weak) {
    if (weak == nullptr) {
        return nullptr;
    }

    return new AIBinder_Weak{weak->binder};
}

bool AIBinder_lt(const AIBinder* lhs, const AIBinder* rhs) {
    if (lhs == nullptr || rhs == nullptr) return lhs < rhs;

    return const_cast<AIBinder*>(lhs)->getBinder() < const_cast<AIBinder*>(rhs)->getBinder();
}

bool AIBinder_Weak_lt(const AIBinder_Weak* lhs, const AIBinder_Weak* rhs) {
    if (lhs == nullptr || rhs == nullptr) return lhs < rhs;

    return lhs->binder < rhs->binder;
}

AIBinder_Class::AIBinder_Class(const char* interfaceDescriptor, AIBinder_Class_onCreate onCreate,
                               AIBinder_Class_onDestroy onDestroy,
                               AIBinder_Class_onTransact onTransact)
    : onCreate(onCreate),
      onDestroy(onDestroy),
      onTransact(onTransact),
      mInterfaceDescriptor(interfaceDescriptor),
      mWideInterfaceDescriptor(interfaceDescriptor) {}

AIBinder_Class* AIBinder_Class_define(const char* interfaceDescriptor,
                                      AIBinder_Class_onCreate onCreate,
                                      AIBinder_Class_onDestroy onDestroy,
                                      AIBinder_Class_onTransact onTransact) {
    if (interfaceDescriptor == nullptr || onCreate == nullptr || onDestroy == nullptr ||
        onTransact == nullptr) {
        return nullptr;
    }

    return new AIBinder_Class(interfaceDescriptor, onCreate, onDestroy, onTransact);
}

void AIBinder_Class_setOnDump(AIBinder_Class* clazz, AIBinder_onDump onDump) {
    CHECK(clazz != nullptr) << "setOnDump requires non-null clazz";

    // this is required to be called before instances are instantiated
    clazz->onDump = onDump;
}

void AIBinder_Class_disableInterfaceTokenHeader(AIBinder_Class* clazz) {
    CHECK(clazz != nullptr) << "disableInterfaceTokenHeader requires non-null clazz";

    clazz->writeHeader = false;
}

void AIBinder_Class_setHandleShellCommand(AIBinder_Class* clazz,
                                          AIBinder_handleShellCommand handleShellCommand) {
    CHECK(clazz != nullptr) << "setHandleShellCommand requires non-null clazz";

    clazz->handleShellCommand = handleShellCommand;
}

const char* AIBinder_Class_getDescriptor(const AIBinder_Class* clazz) {
    CHECK(clazz != nullptr) << "getDescriptor requires non-null clazz";

    return clazz->getInterfaceDescriptorUtf8();
}

AIBinder_DeathRecipient::TransferDeathRecipient::~TransferDeathRecipient() {
    if (mOnUnlinked != nullptr) {
        mOnUnlinked(mCookie);
    }
}

void AIBinder_DeathRecipient::TransferDeathRecipient::binderDied(const wp<IBinder>& who) {
    CHECK(who == mWho) << who.unsafe_get() << "(" << who.get_refs() << ") vs " << mWho.unsafe_get()
                       << " (" << mWho.get_refs() << ")";

    mOnDied(mCookie);

    sp<AIBinder_DeathRecipient> recipient = mParentRecipient.promote();
    sp<IBinder> strongWho = who.promote();

    // otherwise this will be cleaned up later with pruneDeadTransferEntriesLocked
    if (recipient != nullptr && strongWho != nullptr) {
        status_t result = recipient->unlinkToDeath(strongWho, mCookie);
        if (result != ::android::DEAD_OBJECT) {
            LOG(WARNING) << "Unlinking to dead binder resulted in: " << result;
        }
    }

    mWho = nullptr;
}

AIBinder_DeathRecipient::AIBinder_DeathRecipient(AIBinder_DeathRecipient_onBinderDied onDied)
    : mOnDied(onDied), mOnUnlinked(nullptr) {
    CHECK(onDied != nullptr);
}

void AIBinder_DeathRecipient::pruneDeadTransferEntriesLocked() {
    mDeathRecipients.erase(std::remove_if(mDeathRecipients.begin(), mDeathRecipients.end(),
                                          [](const sp<TransferDeathRecipient>& tdr) {
                                              return tdr->getWho() == nullptr;
                                          }),
                           mDeathRecipients.end());
}

binder_status_t AIBinder_DeathRecipient::linkToDeath(const sp<IBinder>& binder, void* cookie) {
    CHECK(binder != nullptr);

    std::lock_guard<std::mutex> l(mDeathRecipientsMutex);

    sp<TransferDeathRecipient> recipient =
            new TransferDeathRecipient(binder, cookie, this, mOnDied, mOnUnlinked);

    status_t status = binder->linkToDeath(recipient, cookie, 0 /*flags*/);
    if (status != STATUS_OK) {
        // When we failed to link, the destructor of TransferDeathRecipient runs here, which
        // ensures that mOnUnlinked is called before we return with an error from this method.
        return PruneStatusT(status);
    }

    mDeathRecipients.push_back(recipient);

    pruneDeadTransferEntriesLocked();
    return STATUS_OK;
}

binder_status_t AIBinder_DeathRecipient::unlinkToDeath(const sp<IBinder>& binder, void* cookie) {
    CHECK(binder != nullptr);

    std::lock_guard<std::mutex> l(mDeathRecipientsMutex);

    for (auto it = mDeathRecipients.rbegin(); it != mDeathRecipients.rend(); ++it) {
        sp<TransferDeathRecipient> recipient = *it;

        if (recipient->getCookie() == cookie && recipient->getWho() == binder) {
            mDeathRecipients.erase(it.base() - 1);

            status_t status = binder->unlinkToDeath(recipient, cookie, 0 /*flags*/);
            if (status != ::android::OK) {
                LOG(ERROR) << __func__
                           << ": removed reference to death recipient but unlink failed.";
            }
            return PruneStatusT(status);
        }
    }

    return STATUS_NAME_NOT_FOUND;
}

void AIBinder_DeathRecipient::setOnUnlinked(AIBinder_DeathRecipient_onBinderUnlinked onUnlinked) {
    mOnUnlinked = onUnlinked;
}

// start of C-API methods

AIBinder* AIBinder_new(const AIBinder_Class* clazz, void* args) {
    if (clazz == nullptr) {
        LOG(ERROR) << __func__ << ": Must provide class to construct local binder.";
        return nullptr;
    }

    void* userData = clazz->onCreate(args);

    sp<AIBinder> ret = new ABBinder(clazz, userData);
    ABBinderTag::attach(ret->getBinder());

    AIBinder_incStrong(ret.get());
    return ret.get();
}

bool AIBinder_isRemote(const AIBinder* binder) {
    if (binder == nullptr) {
        return false;
    }

    return binder->isRemote();
}

bool AIBinder_isAlive(const AIBinder* binder) {
    if (binder == nullptr) {
        return false;
    }

    return const_cast<AIBinder*>(binder)->getBinder()->isBinderAlive();
}

binder_status_t AIBinder_ping(AIBinder* binder) {
    if (binder == nullptr) {
        return STATUS_UNEXPECTED_NULL;
    }

    return PruneStatusT(binder->getBinder()->pingBinder());
}

binder_status_t AIBinder_dump(AIBinder* binder, int fd, const char** args, uint32_t numArgs) {
    if (binder == nullptr) {
        return STATUS_UNEXPECTED_NULL;
    }

    ABBinder* bBinder = binder->asABBinder();
    if (bBinder != nullptr) {
        AIBinder_onDump onDump = binder->getClass()->onDump;
        if (onDump == nullptr) {
            return STATUS_OK;
        }
        return PruneStatusT(onDump(bBinder, fd, args, numArgs));
    }

    ::android::Vector<String16> utf16Args;
    utf16Args.setCapacity(numArgs);
    for (uint32_t i = 0; i < numArgs; i++) {
        utf16Args.push(String16(String8(args[i])));
    }

    status_t status = binder->getBinder()->dump(fd, utf16Args);
    return PruneStatusT(status);
}

binder_status_t AIBinder_linkToDeath(AIBinder* binder, AIBinder_DeathRecipient* recipient,
                                     void* cookie) {
    if (binder == nullptr || recipient == nullptr) {
        LOG(ERROR) << __func__ << ": Must provide binder and recipient.";
        return STATUS_UNEXPECTED_NULL;
    }

    // returns binder_status_t
    return recipient->linkToDeath(binder->getBinder(), cookie);
}

binder_status_t AIBinder_unlinkToDeath(AIBinder* binder, AIBinder_DeathRecipient* recipient,
                                       void* cookie) {
    if (binder == nullptr || recipient == nullptr) {
        LOG(ERROR) << __func__ << ": Must provide binder and recipient.";
        return STATUS_UNEXPECTED_NULL;
    }

    // returns binder_status_t
    return recipient->unlinkToDeath(binder->getBinder(), cookie);
}

uid_t AIBinder_getCallingUid() {
    return ::android::IPCThreadState::self()->getCallingUid();
}

pid_t AIBinder_getCallingPid() {
    return ::android::IPCThreadState::self()->getCallingPid();
}

bool AIBinder_isHandlingTransaction() {
    return ::android::IPCThreadState::self()->getServingStackPointer() != nullptr;
}

void AIBinder_incStrong(AIBinder* binder) {
    if (binder == nullptr) {
        return;
    }

    binder->incStrong(nullptr);
}
void AIBinder_decStrong(AIBinder* binder) {
    if (binder == nullptr) {
        LOG(ERROR) << __func__ << ": on null binder";
        return;
    }

    binder->decStrong(nullptr);
}
int32_t AIBinder_debugGetRefCount(AIBinder* binder) {
    if (binder == nullptr) {
        LOG(ERROR) << __func__ << ": on null binder";
        return -1;
    }

    return binder->getStrongCount();
}

bool AIBinder_associateClass(AIBinder* binder, const AIBinder_Class* clazz) {
    if (binder == nullptr) {
        return false;
    }

    return binder->associateClass(clazz);
}

const AIBinder_Class* AIBinder_getClass(AIBinder* binder) {
    if (binder == nullptr) {
        return nullptr;
    }

    return binder->getClass();
}

void* AIBinder_getUserData(AIBinder* binder) {
    if (binder == nullptr) {
        return nullptr;
    }

    ABBinder* bBinder = binder->asABBinder();
    if (bBinder == nullptr) {
        return nullptr;
    }

    return bBinder->getUserData();
}

binder_status_t AIBinder_prepareTransaction(AIBinder* binder, AParcel** in) {
    if (binder == nullptr || in == nullptr) {
        LOG(ERROR) << __func__ << ": requires non-null parameters.";
        return STATUS_UNEXPECTED_NULL;
    }
    const AIBinder_Class* clazz = binder->getClass();
    if (clazz == nullptr) {
        LOG(ERROR) << __func__
                   << ": Class must be defined for a remote binder transaction. See "
                      "AIBinder_associateClass.";
        return STATUS_INVALID_OPERATION;
    }

    *in = new AParcel(binder);
    (*in)->get()->markForBinder(binder->getBinder());

    status_t status = android::OK;
    if (clazz->writeHeader) {
        status = (*in)->get()->writeInterfaceToken(clazz->getInterfaceDescriptor());
    }
    binder_status_t ret = PruneStatusT(status);

    if (ret != STATUS_OK) {
        delete *in;
        *in = nullptr;
    }

    return ret;
}

static void DestroyParcel(AParcel** parcel) {
    delete *parcel;
    *parcel = nullptr;
}

binder_status_t AIBinder_transact(AIBinder* binder, transaction_code_t code, AParcel** in,
                                  AParcel** out, binder_flags_t flags) {
    if (in == nullptr) {
        LOG(ERROR) << __func__ << ": requires non-null in parameter";
        return STATUS_UNEXPECTED_NULL;
    }

    using AutoParcelDestroyer = std::unique_ptr<AParcel*, void (*)(AParcel**)>;
    // This object is the input to the transaction. This function takes ownership of it and deletes
    // it.
    AutoParcelDestroyer forIn(in, DestroyParcel);

    if (!isUserCommand(code)) {
        LOG(ERROR) << __func__ << ": Only user-defined transactions can be made from the NDK.";
        return STATUS_UNKNOWN_TRANSACTION;
    }

    constexpr binder_flags_t kAllFlags = static_cast<uint32_t>(FLAG_PRIVATE_VENDOR) |
                                         static_cast<uint32_t>(FLAG_ONEWAY) |
                                         static_cast<uint32_t>(FLAG_CLEAR_BUF);
    if ((flags & ~kAllFlags) != 0) {
        LOG(ERROR) << __func__ << ": Unrecognized flags sent: " << flags;
        return STATUS_BAD_VALUE;
    }

    if (binder == nullptr || *in == nullptr || out == nullptr) {
        LOG(ERROR) << __func__ << ": requires non-null parameters.";
        return STATUS_UNEXPECTED_NULL;
    }

    if ((*in)->getBinder() != binder) {
        LOG(ERROR) << __func__ << ": parcel is associated with binder object " << binder
                   << " but called with " << (*in)->getBinder();
        return STATUS_BAD_VALUE;
    }

    *out = new AParcel(binder);

    status_t status = binder->getBinder()->transact(code, *(*in)->get(), (*out)->get(), flags);
    binder_status_t ret = PruneStatusT(status);

    if (ret != STATUS_OK) {
        delete *out;
        *out = nullptr;
    }

    return ret;
}

AIBinder_DeathRecipient* AIBinder_DeathRecipient_new(
        AIBinder_DeathRecipient_onBinderDied onBinderDied) {
    if (onBinderDied == nullptr) {
        LOG(ERROR) << __func__ << ": requires non-null onBinderDied parameter.";
        return nullptr;
    }
    auto ret = new AIBinder_DeathRecipient(onBinderDied);
    ret->incStrong(nullptr);
    return ret;
}

void AIBinder_DeathRecipient_setOnUnlinked(AIBinder_DeathRecipient* recipient,
                                           AIBinder_DeathRecipient_onBinderUnlinked onUnlinked) {
    if (recipient == nullptr) {
        return;
    }

    recipient->setOnUnlinked(onUnlinked);
}

void AIBinder_DeathRecipient_delete(AIBinder_DeathRecipient* recipient) {
    if (recipient == nullptr) {
        return;
    }

    recipient->decStrong(nullptr);
}

binder_status_t AIBinder_getExtension(AIBinder* binder, AIBinder** outExt) {
    if (binder == nullptr || outExt == nullptr) {
        if (outExt != nullptr) {
            *outExt = nullptr;
        }
        return STATUS_UNEXPECTED_NULL;
    }

    sp<IBinder> ext;
    status_t res = binder->getBinder()->getExtension(&ext);

    if (res != android::OK) {
        *outExt = nullptr;
        return PruneStatusT(res);
    }

    sp<AIBinder> ret = ABpBinder::lookupOrCreateFromBinder(ext);
    if (ret != nullptr) ret->incStrong(binder);

    *outExt = ret.get();
    return STATUS_OK;
}

binder_status_t AIBinder_setExtension(AIBinder* binder, AIBinder* ext) {
    if (binder == nullptr || ext == nullptr) {
        return STATUS_UNEXPECTED_NULL;
    }

    ABBinder* rawBinder = binder->asABBinder();
    if (rawBinder == nullptr) {
        return STATUS_INVALID_OPERATION;
    }

    rawBinder->setExtension(ext->getBinder());
    return STATUS_OK;
}

// platform methods follow

void AIBinder_setRequestingSid(AIBinder* binder, bool requestingSid) {
    ABBinder* localBinder = binder->asABBinder();
    if (localBinder == nullptr) {
        LOG(FATAL) << "AIBinder_setRequestingSid must be called on a local binder";
    }

    localBinder->setRequestingSid(requestingSid);
}

const char* AIBinder_getCallingSid() {
    return ::android::IPCThreadState::self()->getCallingSid();
}

android::sp<android::IBinder> AIBinder_toPlatformBinder(AIBinder* binder) {
    if (binder == nullptr) return nullptr;
    return binder->getBinder();
}

AIBinder* AIBinder_fromPlatformBinder(const android::sp<android::IBinder>& binder) {
    sp<AIBinder> ndkBinder = ABpBinder::lookupOrCreateFromBinder(binder);
    AIBinder_incStrong(ndkBinder.get());
    return ndkBinder.get();
}

void AIBinder_setMinSchedulerPolicy(AIBinder* binder, int policy, int priority) {
    binder->asABBinder()->setMinSchedulerPolicy(policy, priority);
}

void AIBinder_setInheritRt(AIBinder* binder, bool inheritRt) {
    ABBinder* localBinder = binder->asABBinder();
    if (localBinder == nullptr) {
        LOG(FATAL) << "AIBinder_setInheritRt must be called on a local binder";
    }

    localBinder->setInheritRt(inheritRt);
}
