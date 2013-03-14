/*
 * Copyright (C) 2008 The Android Open Source Project
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

#define LOG_TAG "MemoryBase"

#include <stdlib.h>
#include <stdint.h>

#include <binder/MemoryBase.h>


namespace android {

// ---------------------------------------------------------------------------

MemoryBase::MemoryBase(const sp<IMemoryHeap>& heap,
        ssize_t offset, size_t size)
    : mSize(size), mOffset(offset), mHeap(heap)
{
}

MemoryBase::MemoryBase(const sp<IMemoryHeap>& heap,
        long offset, unsigned int size)
    : mSize(size), mOffset(offset), mHeap(heap)
{
    ALOGW("Using temporary compatibility workaround for usage of MemoryBase "
          "private API. Please fix your application!");
}

sp<IMemoryHeap> MemoryBase::getMemory(ssize_t* offset, size_t* size) const
{
    if (offset) *offset = mOffset;
    if (size)   *size = mSize;
    return mHeap;
}

MemoryBase::~MemoryBase()
{
}

// ---------------------------------------------------------------------------
}; // namespace android
