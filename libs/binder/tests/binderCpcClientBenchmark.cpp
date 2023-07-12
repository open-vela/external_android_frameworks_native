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
#include <binder/RpcSession.h>
#include <binder/RpcTransportRaw.h>
#include <sys/types.h>
#include <unistd.h>

using android::BBinder;
using android::IBinder;
using android::interface_cast;
using android::RpcSession;
using android::sp;
using android::status_t;
using android::statusToString;
using android::binder::Status;

static sp<RpcSession> gRpcSession = RpcSession::make();

void BM_throughputForTransportAndBytes(benchmark::State& state)
{
    CHECK(gRpcSession != nullptr);
    sp<IBinder> binder = gRpcSession->getRootObject();
    if (binder == nullptr) {
        LOG(FATAL) << "RpcSession getRootObject failed";
        return;
    }

    sp<IBinderRpcBenchmark> iface = interface_cast<IBinderRpcBenchmark>(binder);
    CHECK(iface != nullptr);

    std::vector<uint8_t> bytes = std::vector<uint8_t>(state.range(0));
    for (size_t i = 0; i < bytes.size(); i++) {
        bytes[i] = i % 256;
    }
    while (state.KeepRunning()) {
        std::vector<uint8_t> out;
        Status ret = iface->repeatBytes(bytes, &out);
        CHECK(ret.isOk()) << ret;
    }
}
BENCHMARK(BM_throughputForTransportAndBytes)
    ->ArgsProduct({ { 64, 128, 256, 512, 1024, 2048, 4096, 8182 } });

void setupClient(const sp<RpcSession>& session)
{
    session->setupRpmsgSockClient("ap", "cpc");
}

extern "C" int main(int argc, char** argv)
{
    ::benchmark::Initialize(&argc, argv);

    setupClient(gRpcSession);

    ::benchmark::RunSpecifiedBenchmarks();

    return 0;
}
