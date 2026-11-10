// Copyright (c) 2026 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <mfx_scheduler_core_next_factory.h>
#include <mfx_platform_caps.h>
#include <mfx_trace.h>
#include <mfxstructures-int.h>

#include <algorithm>
#include <iterator>



#include <stdio.h>
#include <string>

namespace mfx_sched_next
{


static bool IsSchedulerNextDevice(mfxU32 deviceId)
{

    auto it = std::find_if(std::begin(listLegalDevIDs), std::end(listLegalDevIDs),
        [deviceId](const mfx_device_item& item) { return mfxU32(item.device_id) == deviceId; });

    // unknown device IDs keep the legacy scheduler
    return it != std::end(listLegalDevIDs) && SchedulerCaps::IsSchedulerNextSupported(it->platform);

} // static bool IsSchedulerNextDevice(mfxU32 deviceId)

bool IsNextPlatformSet(const std::vector<mfxU32>& intelDeviceIds)
{

    return !intelDeviceIds.empty()
        && std::all_of(intelDeviceIds.begin(), intelDeviceIds.end(), IsSchedulerNextDevice);

} // bool IsNextPlatformSet(const std::vector<mfxU32>& intelDeviceIds)

// Device IDs of all Intel GPUs in the system, empty on any failure
static std::vector<mfxU32> EnumerateIntelDeviceIds(void)
{

    std::vector<mfxU32> deviceIds;


    for (int i = 0; i < 64; ++i)
    {
        const std::string path = std::string("/sys/class/drm/renderD") + std::to_string(128 + i) + "/device/";

        mfxU32 vendorId = 0;
        FILE* file = fopen((path + "vendor").c_str(), "r");
        if (!file)
            continue;

        int nread = fscanf(file, "%x", &vendorId);
        fclose(file);

        if (nread != 1 || vendorId != 0x8086)
            continue;

        mfxU32 deviceId = 0;
        file = fopen((path + "device").c_str(), "r");
        if (!file)
            return {};

        nread = fscanf(file, "%x", &deviceId);
        fclose(file);

        if (nread != 1)
            return {};

        deviceIds.push_back(deviceId);
    }

    return deviceIds;

} // static std::vector<mfxU32> EnumerateIntelDeviceIds(void)


static bool Select(void)
{

    bool isNext = false;

    {
        // detection failure (e.g. std::bad_alloc) keeps the legacy scheduler
        try
        {
            isNext = IsNextPlatformSet(EnumerateIntelDeviceIds());
        }
        catch (...)
        {
            isNext = false;
        }
    }

    MFX_LTRACE_1(MFX_TRACE_LEVEL_SCHED, "Scheduler Next selected = ", MFX_TRACE_FORMAT_I, int(isNext));

    return isNext;

} // static bool Select(void)

bool IsSelected(void)
{

    // thread-safe one-time initialization
    static const bool isSelected = Select();
    return isSelected;

} // bool IsSelected(void)

} // namespace mfx_sched_next
