/*
 * Copyright (C) 2009 The Android Open Source Project
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

/*
 * Command that dumps interesting system state to the log.
 */

#include "dumpsys.h"

#include <binder/IServiceManager.h>

#include <iostream>
#include <signal.h>

using namespace android;

extern "C" int main(int argc, char* const argv[]) {
    signal(SIGPIPE, SIG_IGN);
    sp<IServiceManager> sm = defaultServiceManager();
    fflush(stdout);
    if (sm == nullptr) {
        ALOGE("Unable to get default service manager!");
        std::cerr << "dumpsys: Unable to get default service manager!" << std::endl;
        return 20;
    }

    Dumpsys dumpsys(sm.get());
    return dumpsys.main(argc, argv);
}
