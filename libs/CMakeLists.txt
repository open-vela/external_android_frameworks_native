#
# Copyright (C) 2023 Xiaomi Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
if(CONFIG_ANDROID_BINDER)

  # binder global include path
  set(BINDER_GLOBAL_C_INCDIR
      ${CMAKE_CURRENT_LIST_DIR}/binder/include_rpc_unstable
      ${CMAKE_CURRENT_LIST_DIR}/binder/ndk/include_ndk
      ${CMAKE_CURRENT_LIST_DIR}/binder/ndk/include_platform)

  set(BINDER_GLOBAL_CXX_INCDIR
      ${CMAKE_CURRENT_LIST_DIR}/binder/aidl
      ${CMAKE_CURRENT_LIST_DIR}/binder/include
      ${CMAKE_CURRENT_LIST_DIR}/binder/include_activitymanager
      ${CMAKE_CURRENT_LIST_DIR}/binder/include_batterystats
      ${CMAKE_CURRENT_LIST_DIR}/binder/include_processinfo
      ${CMAKE_CURRENT_LIST_DIR}/binder/ndk/include_cpp
      ${CMAKE_CURRENT_LIST_DIR}/binderdebug/include)

  set_property(
    TARGET nuttx
    APPEND
    PROPERTY NUTTX_CXX_INCLUDE_DIRECTORIES ${BINDER_GLOBAL_CXX_INCDIR})

  set_property(
    TARGET nuttx
    APPEND
    PROPERTY NUTTX_INCLUDE_DIRECTORIES ${BINDER_GLOBAL_C_INCDIR})

  nuttx_add_library(android_binder STATIC)

  # sources

  set(SRCS
      binder/ndk/ibinder.cpp
      binder/ndk/parcel.cpp
      binder/ndk/process.cpp
      binder/ndk/service_manager.cpp
      binder/ndk/stability.cpp
      binder/ndk/status.cpp
      binder/ActivityManager.cpp
      binder/Binder.cpp
      binder/BpBinder.cpp
      binder/BufferedTextOutput.cpp
      binder/Debug.cpp
      binder/FdTrigger.cpp
      binder/IActivityManager.cpp
      binder/IBatteryStats.cpp
      binder/IInterface.cpp
      binder/IMemory.cpp
      binder/IPCThreadState.cpp
      binder/IPermissionController.cpp
      binder/IProcessInfoService.cpp
      binder/IResultReceiver.cpp
      binder/IServiceManager.cpp
      binder/IShellCallback.cpp
      binder/IUidObserver.cpp
      binder/LazyServiceRegistrar.cpp
      binder/libbinder_rpc_unstable.cpp
      binder/MemoryBase.cpp
      binder/MemoryDealer.cpp
      binder/MemoryHeapBase.cpp
      binder/Parcel.cpp
      binder/ParcelableHolder.cpp
      binder/ParcelFileDescriptor.cpp
      binder/PermissionCache.cpp
      binder/PermissionController.cpp
      binder/PersistableBundle.cpp
      binder/ProcessInfoService.cpp
      binder/ProcessState.cpp
      binder/RpcServer.cpp
      binder/RpcSession.cpp
      binder/RpcState.cpp
      binder/RpcTransportRaw.cpp
      binder/Stability.cpp
      binder/Static.cpp
      binder/Status.cpp
      binder/TextOutput.cpp
      binder/Utils.cpp
      binderdebug/BinderDebug.cpp)

  # aidl sources
  file(GLOB_RECURSE AIDLS binder/aidl/*.aidl)
  nuttx_add_aidl(
    TARGET
    android_binder
    AIDL_BASE_DIR
    ${CMAKE_CURRENT_LIST_DIR}/binder/aidl
    AIDL_INCLUDE_DIR
    ${CMAKE_CURRENT_LIST_DIR}/binder/aidl
    AIDL_HEADER_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/binder/aidl
    AIDL_OUT_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/binder/aidl
    AIDL_FLAGS
    --lang=cpp
    AIDLS
    ${AIDLS})

  # flags
  set(FLAGS -Wno-undef -Wno-shadow -Wno-unknown-pragmas)

  if(NOT DEFINED GCCVER)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} --version
                    OUTPUT_VARIABLE GCC_VERSION_OUTPUT)
    string(REGEX MATCH "\\+\\+.* ([0-9]+)\\.[0-9]+" GCC_VERSION_REGEX
                 "${GCC_VERSION_OUTPUT}")
    set(GCCVER ${CMAKE_MATCH_1})
  endif()

  if(GCCVER EQUAL 12)
    list(APPEND FLAGS -Wno-maybe-uninitialized)
  endif()

  macro(add_binder_test_app PROG MAIN)
    nuttx_add_application(
      MODULE
      ${CONFIG_ANDROID_BINDER_TEST}
      NAME
      ${PROG}
      STACKSIZE
      ${CONFIG_ANDROID_BINDER_TEST_STACKSIZE}
      SRCS
      ${MAIN}
      COMPILE_FLAGS
      ${FLAGS}
      DEPENDS
      android_binder)
  endmacro()

  if(CONFIG_ANDROID_BINDER_DRIVER_TEST)
    add_binder_test_app(binderLibTest binder/tests/binderLibTest.cpp)
  endif()

  if(CONFIG_ANDROID_BINDER_DRIVER_TEST)
    add_binder_test_app(binderDrvTest
                        binder/tests/binderDriverInterfaceTest.cpp)
  endif()

  if(CONFIG_ANDROID_BINDER_PERFORMANCE_TEST)
    add_binder_test_app(schdDbg binder/tests/schd-dbg.cpp)
  endif()

  if(CONFIG_ANDROID_BINDER_XPC_PERFORMANCE_TEST)
    nuttx_add_aidl(
      TARGET
      android_binder
      AIDL_BASE_DIR
      ${CMAKE_CURRENT_LIST_DIR}/binder/test
      AIDL_INCLUDE_DIR
      ${CMAKE_CURRENT_LIST_DIR}/binder/aidl
      AIDL_HEADER_DIR
      ${CMAKE_CURRENT_BINARY_DIR}/binder/aidl
      AIDL_OUT_DIR
      ${CMAKE_CURRENT_BINARY_DIR}/binder/test
      AIDL_FLAGS
      --lang=cpp
      AIDLS
      binder/tests/IBinderRpcBenchmark.aidl)
  endif()

  if(CONFIG_ANDROID_BINDER_CPC_PERFORMANCE_TEST)
    add_binder_test_app(cpcServerBenchmark
                        binder/tests/binderCpcServerBenchmark.cpp)
    add_binder_test_app(cpcClientBenchmark
                        binder/tests/binderCpcClientBenchmark.cpp)
  endif()

  if(CONFIG_ANDROID_BINDER_RPC_PERFORMANCE_TEST)
    add_binder_test_app(rpcBenchmark binder/tests/binderRpcBenchmark.cpp)
  endif()

  if(CONFIG_RPMSGSOCK_PERFORMANCE_TEST)
    add_binder_test_app(rpSockClient binder/tests/rpmsgClientBench.cpp)
    add_binder_test_app(rpSockServer binder/tests/rpmsgServerBench.cpp)
  endif()

  target_sources(android_binder PRIVATE ${SRCS})
  target_compile_options(android_binder PRIVATE ${FLAGS})

endif()
