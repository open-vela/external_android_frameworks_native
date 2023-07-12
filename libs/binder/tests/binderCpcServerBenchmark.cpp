/*
 * Copyright (C) 2021 The Android Open Source Project
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
#include <BnBinderRpcBenchmark.h>
#include <android-base/logging.h>
#include <benchmark/benchmark.h>
#include <binder/Binder.h>
#include <binder/RpcServer.h>
#include <binder/RpcTransportRaw.h>
#include <sys/types.h>
#include <unistd.h>

using android::BBinder;
using android::IBinder;
using android::interface_cast;
using android::RpcServer;
using android::sp;
using android::status_t;
using android::statusToString;
using android::binder::Status;

class MyBinderRpcBenchmark : public BnBinderRpcBenchmark {
    Status repeatString(const std::string& str, std::string* out) override
    {
        *out = str;
        return Status::ok();
    }
    Status repeatBinder(const sp<IBinder>& binder, sp<IBinder>* out) override
    {
        *out = binder;
        return Status::ok();
    }
    Status repeatBytes(const std::vector<uint8_t>& bytes, std::vector<uint8_t>* out) override
    {
        *out = bytes;
        return Status::ok();
    }
};

void defaultCpcServer(const sp<RpcServer>& server)
{
    server->setRootObject(sp<MyBinderRpcBenchmark>::make());
    CHECK_EQ(OK, server->setupRpmsgSockServer("cpc"));
    server->join();
}

extern "C" int main(int argc, char** argv)
{
    defaultCpcServer(RpcServer::make());
    return 0;
}
