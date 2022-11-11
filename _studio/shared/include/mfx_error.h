// Copyright (c) 2020 Intel Corporation
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

#pragma once

#include "mfx_config.h"

#include <algorithm>
#include <system_error>

namespace mfx
{
    struct error_category
        : std::error_category
    {
        const char* name() const noexcept override
        { return "MFX"; }

        std::string message(int code) const noexcept override
        {
            struct message
            {
                mfxStatus   code;
                char const* description;
            } const messages[] =
            {
                //from "mfxdefs.h"
                { MFX_ERR_NONE,                        "MFX_ERR_NONE" }, // : no error
                { MFX_ERR_UNKNOWN,                     "MFX_ERR_UNKNOWN" }, // : unknown error
                { MFX_ERR_NULL_PTR,                    "MFX_ERR_NULL_PTR" }, // : null pointer
                { MFX_ERR_UNSUPPORTED,                 "MFX_ERR_UNSUPPORTED" }, // : undeveloped feature
                { MFX_ERR_MEMORY_ALLOC,                "MFX_ERR_MEMORY_ALLOC" }, // : failed to allocate memory
                { MFX_ERR_NOT_ENOUGH_BUFFER,           "MFX_ERR_NOT_ENOUGH_BUFFER" }, // : insufficient buffer at input/output
                { MFX_ERR_INVALID_HANDLE,              "MFX_ERR_INVALID_HANDLE" }, // : invalid handle
                { MFX_ERR_LOCK_MEMORY,                 "MFX_ERR_LOCK_MEMORY" }, // : failed to lock the memory block
                { MFX_ERR_NOT_INITIALIZED,             "MFX_ERR_NOT_INITIALIZED" }, // : member function called before initialization
                { MFX_ERR_NOT_FOUND,                   "MFX_ERR_NOT_FOUND" }, // : the specified object is not found
                { MFX_ERR_MORE_DATA,                   "MFX_ERR_MORE_DATA" }, // : expect more data at input
                { MFX_ERR_MORE_SURFACE,                "MFX_ERR_MORE_SURFACE" }, // : expect more surface at output
                { MFX_ERR_ABORTED,                     "MFX_ERR_ABORTED" }, // : operation aborted
                { MFX_ERR_DEVICE_LOST,                 "MFX_ERR_DEVICE_LOST" }, // : lose the HW acceleration device
                { MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,    "MFX_ERR_INCOMPATIBLE_VIDEO_PARAM" }, // : incompatible video parameters
                { MFX_ERR_INVALID_VIDEO_PARAM,         "MFX_ERR_INVALID_VIDEO_PARAM" }, // : invalid video parameters
                { MFX_ERR_UNDEFINED_BEHAVIOR,          "MFX_ERR_UNDEFINED_BEHAVIOR" }, // : undefined behavior
                { MFX_ERR_DEVICE_FAILED,               "MFX_ERR_DEVICE_FAILED" }, // : device operation failure
                { MFX_ERR_MORE_BITSTREAM,              "MFX_ERR_MORE_BITSTREAM" }, // : expect more bitstream buffers at output
                { MFX_ERR_GPU_HANG,                    "MFX_ERR_GPU_HANG" }, // : device operation failure caused by GPU hang
                { MFX_ERR_REALLOC_SURFACE,             "MFX_ERR_REALLOC_SURFACE" }, // : bigger output surface required
                { MFX_ERR_RESOURCE_MAPPED,             "MFX_ERR_RESOURCE_MAPPED"}, //: write access is already acquired and user requested another write access, or read access with MFX_MEMORY_NO_WAIT flag"
                { MFX_WRN_IN_EXECUTION,                "MFX_WRN_IN_EXECUTION" }, // : the previous asynchronous operation is in execution
                { MFX_WRN_DEVICE_BUSY,                 "MFX_WRN_DEVICE_BUSY" }, // : the HW acceleration device is busy
                { MFX_WRN_VIDEO_PARAM_CHANGED,         "MFX_WRN_VIDEO_PARAM_CHANGED" }, // : the video parameters are changed during decoding
                { MFX_WRN_PARTIAL_ACCELERATION,        "MFX_WRN_PARTIAL_ACCELERATION" }, // : SW is used
                { MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,    "MFX_WRN_INCOMPATIBLE_VIDEO_PARAM" }, // : incompatible video parameters
                { MFX_WRN_VALUE_NOT_CHANGED,           "MFX_WRN_VALUE_NOT_CHANGED" }, // : the value is saturated based on its valid range
                { MFX_WRN_OUT_OF_RANGE,                "MFX_WRN_OUT_OF_RANGE" }, // : the value is out of valid range
                { MFX_WRN_FILTER_SKIPPED,              "MFX_WRN_FILTER_SKIPPED" }, // : one of requested filters has been skipped

                { MFX_ERR_NONE_PARTIAL_OUTPUT,         "MFX_ERR_NONE_PARTIAL_OUTPUT" }, // : frame is not ready, but bitstream contains partial output
                { MFX_TASK_WORKING,                    "MFX_TASK_WORKING" }, // : there is some more work to do
                { MFX_TASK_BUSY,                       "MFX_TASK_BUSY" }, // : task is waiting for resources
                { MFX_ERR_MORE_DATA_SUBMIT_TASK,       "MFX_ERR_MORE_DATA_SUBMIT_TASK" } // : return MFX_ERR_MORE_DATA but submit internal asynchronous task
            };

            auto m = std::find_if(std::begin(messages), std::end(messages),
                [code](message const& m) { return m.code == code; }
            );

            return
                m != std::end(messages) ? (*m).description : "";
        }

        std::error_condition default_error_condition(int value) const noexcept override
        {
            switch (value)
            {
                case MFX_ERR_UNSUPPORTED:         return std::errc::not_supported;

                case MFX_ERR_NULL_PTR:
                case MFX_ERR_INVALID_HANDLE:      return std::errc::bad_address;

                case MFX_ERR_MEMORY_ALLOC:        return std::errc::not_enough_memory;

                case MFX_ERR_NOT_ENOUGH_BUFFER:
                case MFX_ERR_MORE_BITSTREAM:      return std::errc::no_buffer_space;

                case MFX_ERR_NOT_FOUND:           return std::errc::no_such_file_or_directory;
                case MFX_ERR_ABORTED:             return std::errc::operation_canceled;

                case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
                case MFX_ERR_INVALID_VIDEO_PARAM: return std::errc::invalid_argument;

                case MFX_ERR_UNDEFINED_BEHAVIOR:
                case MFX_ERR_DEVICE_FAILED:       return std::errc::state_not_recoverable;

                case MFX_ERR_REALLOC_SURFACE:     return std::errc::message_size;

                case MFX_ERR_RESOURCE_MAPPED:     return std::errc::resource_deadlock_would_occur;

                case MFX_ERR_GPU_HANG:            return std::errc::device_or_resource_busy;

                default: return std::error_condition(value, *this);
            }
        }
    };

    inline
    std::error_category const& category()
    {
        static const error_category c;
        return c;
    }

    inline
    std::error_code make_error_code(mfxStatus status)
    {
        return std::error_code(int(status), category());
    }

    inline
    std::error_condition make_error_condition(mfxStatus status)
    {
        return std::error_condition(int(status), category());
    }
}
