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

#pragma once

#include <stdint.h>
#include <sys/cdefs.h>

#include <android/binder_parcel.h>
#include <android/binder_status.h>

__BEGIN_DECLS

// Also see TF_* in kernel's binder.h
typedef uint32_t binder_flags_t;
enum {
    /**
     * The transaction will be dispatched and then returned to the caller. The outgoing process
     * cannot block a call made by this, and execution of the call will not be waited on. An error
     * can still be returned if the call is unable to be processed by the binder driver. All oneway
     * calls are guaranteed to be ordered if they are sent on the same AIBinder object.
     */
    FLAG_ONEWAY = 0x01,
};

// Also see IBinder.h in libbinder
typedef uint32_t transaction_code_t;
enum {
    /**
     * The first transaction code available for user commands (inclusive).
     */
    FIRST_CALL_TRANSACTION = 0x00000001,
    /**
     * The last transaction code available for user commands (inclusive).
     */
    LAST_CALL_TRANSACTION = 0x00ffffff,
};

/**
 * Represents a type of AIBinder object which can be sent out.
 */
struct AIBinder_Class;
typedef struct AIBinder_Class AIBinder_Class;

/**
 * Represents a local or remote object which can be used for IPC or which can itself be sent.
 *
 * This object has a refcount associated with it and will be deleted when its refcount reaches zero.
 * How methods interactive with this refcount is described below. When using this API, it is
 * intended for a client of a service to hold a strong reference to that service. This also means
 * that user data typically should hold a strong reference to a local AIBinder object. A remote
 * AIBinder object automatically holds a strong reference to the AIBinder object in the server's
 * process. A typically memory layout looks like this:
 *
 * Key:
 *   --->         Ownership/a strong reference
 *   ...>         A weak reference
 *
 *                         (process boundary)
 *                                 |
 * MyInterface ---> AIBinder_Weak  |  ProxyForMyInterface
 *      ^                .         |          |
 *      |                .         |          |
 *      |                v         |          v
 *   UserData  <---   AIBinder   <-|-      AIBinder
 *                                 |
 *
 * In this way, you'll notice that a proxy for the interface holds a strong reference to the
 * implementation and that in the server process, the AIBinder object which was sent can be resent
 * so that the same AIBinder object always represents the same object. This allows, for instance, an
 * implementation (usually a callback) to transfer all ownership to a remote process and
 * automatically be deleted when the remote process is done with it or dies. Other memory models are
 * possible, but this is the standard one.
 */
struct AIBinder;
typedef struct AIBinder AIBinder;

/**
 * The AIBinder object associated with this can be retrieved if it is still alive so that it can be
 * re-used. The intention of this is to enable the same AIBinder object to always represent the same
 * object.
 */
struct AIBinder_Weak;
typedef struct AIBinder_Weak AIBinder_Weak;

/**
 * This is called whenever a new AIBinder object is needed of a specific class.
 *
 * These arguments are passed from AIBinder_new. The return value is stored and can be retrieved
 * using AIBinder_getUserData.
 */
typedef void* (*AIBinder_Class_onCreate)(void* args);

/**
 * This is called whenever an AIBinder object is no longer referenced and needs destroyed.
 *
 * Typically, this just deletes whatever the implementation is.
 */
typedef void (*AIBinder_Class_onDestroy)(void* userData);

/**
 * This is called whenever a transaction needs to be processed by a local implementation.
 */
typedef binder_status_t (*AIBinder_Class_onTransact)(AIBinder* binder, transaction_code_t code,
                                                     const AParcel* in, AParcel* out);

/**
 * An interfaceDescriptor uniquely identifies the type of object that is being created. This is used
 * internally for sanity checks on transactions.
 *
 * None of these parameters can be nullptr.
 *
 * This is created one time during library initialization and cleaned up when the process exits or
 * execs.
 */
__attribute__((warn_unused_result)) AIBinder_Class* AIBinder_Class_define(
        const char* interfaceDescriptor, AIBinder_Class_onCreate onCreate,
        AIBinder_Class_onDestroy onDestroy, AIBinder_Class_onTransact onTransact);

/**
 * Creates a new binder object of the appropriate class.
 *
 * Ownership of args is passed to this object. The lifecycle is implemented with AIBinder_incStrong
 * and AIBinder_decStrong. When the reference count reaches zero, onDestroy is called.
 *
 * When this is called, the refcount is implicitly 1. So, calling decStrong exactly one time is
 * required to delete this object.
 */
__attribute__((warn_unused_result)) AIBinder* AIBinder_new(const AIBinder_Class* clazz, void* args);

/**
 * If this is hosted in a process other than the current one.
 */
bool AIBinder_isRemote(const AIBinder* binder);

/**
 * This can only be called if a strong reference to this object already exists in process.
 */
void AIBinder_incStrong(AIBinder* binder);

/**
 * This will delete the object and call onDestroy once the refcount reaches zero.
 */
void AIBinder_decStrong(AIBinder* binder);

/**
 * For debugging only!
 */
int32_t AIBinder_debugGetRefCount(AIBinder* binder);

/**
 * This sets the class of an AIBinder object. This checks to make sure the remote object is of
 * the expected class. A class must be set in order to use transactions on an AIBinder object.
 * However, if an object is just intended to be passed through to another process or used as a
 * handle this need not be called.
 *
 * This returns true if the class association succeeds. If it fails, no change is made to the
 * binder object.
 */
bool AIBinder_associateClass(AIBinder* binder, const AIBinder_Class* clazz);

/*
 * Returns the class that this binder was constructed with or associated with.
 */
const AIBinder_Class* AIBinder_getClass(AIBinder* binder);

/**
 * Value returned by onCreate for a local binder. For stateless classes (if onCreate returns
 * nullptr), this also returns nullptr. For a remote binder, this will always return nullptr.
 */
void* AIBinder_getUserData(AIBinder* binder);

/**
 * A transaction is a series of calls to these functions which looks this
 * - call AIBinder_prepareTransaction
 * - fill out the in parcel with parameters (lifetime of the 'in' variable)
 * - call AIBinder_transact
 * - read results from the out parcel (lifetime of the 'out' variable)
 */

/**
 * Creates a parcel to start filling out for a transaction. This may add data to the parcel for
 * security, debugging, or other purposes. This parcel is to be sent via AIBinder_transact and it
 * represents the input data to the transaction. It is recommended to check if the object is local
 * and call directly into its user data before calling this as the parceling and unparceling cost
 * can be avoided. This AIBinder must be either built with a class or associated with a class before
 * using this API.
 *
 * This does not affect the ownership of binder. When this function succeeds, the in parcel's
 * ownership is passed to the caller. At this point, the parcel can be filled out and passed to
 * AIBinder_transact. Alternatively, if there is an error while filling out the parcel, it can be
 * deleted with AParcel_delete.
 */
binder_status_t AIBinder_prepareTransaction(AIBinder* binder, AParcel** in);

/**
 * Transact using a parcel created from AIBinder_prepareTransaction. This actually communicates with
 * the object representing this binder object. This also passes out a parcel to be used for the
 * return transaction. This takes ownership of the in parcel and automatically deletes it after it
 * is sent to the remote process. The output parcel is the result of the transaction. If the
 * transaction has FLAG_ONEWAY, the out parcel will be empty. Otherwise, this will block until the
 * remote process has processed the transaction, and the out parcel will contain the output data
 * from transaction.
 *
 * This does not affect the ownership of binder. The out parcel's ownership is passed to the caller
 * and must be released with AParcel_delete when finished reading.
 */
binder_status_t AIBinder_transact(AIBinder* binder, transaction_code_t code, AParcel** in,
                                  AParcel** out, binder_flags_t flags);

/*
 * This does not take any ownership of the input binder, but it can be used to retrieve it if
 * something else in some process still holds a reference to it.
 */
__attribute__((warn_unused_result)) AIBinder_Weak* AIBinder_Weak_new(AIBinder* binder);

/*
 * Deletes the weak reference. This will have no impact on the lifetime of the binder.
 */
void AIBinder_Weak_delete(AIBinder_Weak** weakBinder);

/**
 * If promotion succeeds, result will have one strong refcount added to it. Otherwise, this returns
 * nullptr.
 */
__attribute__((warn_unused_result)) AIBinder* AIBinder_Weak_promote(AIBinder_Weak* weakBinder);

__END_DECLS
