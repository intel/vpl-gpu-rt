// Copyright (c) 2008-2020 Intel Corporation
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

#ifndef __MFXUTILS_H__
#define __MFXUTILS_H__

#include "mfx_config.h"

#include "mfxstructures.h"

#include "mfxdeprecated.h"
#include "mfxplugin.h"

#include "umc_structures.h"
#include "mfx_trace.h"
#include "mfx_utils_logging.h"
#include "mfx_utils_perf.h"
#include "mfx_decode_dpb_logging.h"
#include "mfx_timing.h"
#include "mfxsurfacepool.h"
#include "mfx_error.h"

#include <va/va.h>
#include <dlfcn.h>


#include <cassert>
#include <cstddef>
#include <algorithm>
#include <chrono>
#include <functional>
#include <atomic>
#include <sstream>
#include <utility>
#include <malloc.h>
#include <cstdlib>


// MFX_LOG output to the console in Debug mode;
// MFX_LTRACE output to vpl log file in Release and Debug modes.
template <
    typename T
    , typename std::enable_if<!std::is_same<T, mfxStatus>::value, int>::type = 0>
static inline T mfx_sts_trace(const char* fileName, const uint32_t lineNum, const char* funcName, T sts)
{
    const int mSts = static_cast<int>(sts);
#if defined(MFX_ENABLE_LOG_UTILITY)
    if (mSts)
    {
        MFX_LOG(LEVEL_ERROR, fileName, lineNum, "%s: returns %d\n", funcName, sts);
    }
#endif
    if (mSts != 0)
    {
        std::string mfxSts = (mSts > 0) ? "[warning]  Status = " : "[critical]  Status = ";

        MFX_LTRACE((&_trace_static_handle, fileName, lineNum, funcName, MFX_TRACE_CATEGORY, MFX_TRACE_LEVEL_INTERNAL, mfxSts.c_str(), MFX_TRACE_FORMAT_I, sts));
    }

    return sts;
}

template <
    typename T
    , typename = typename std::enable_if<std::is_same<T, mfxStatus>::value>::type>
static inline T mfx_sts_trace(const char* fileName, const uint32_t lineNum, const char* funcName, T sts)
{
    const std::string stsString = GetMFXStatusInString(sts);
    std::string mfxSts;
    if (sts > MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA || sts == MFX_ERR_MORE_SURFACE || sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) //MFX_ERR_MORE_DATA, MFX_ERR_MORE_SURFACE and MFX_ERR_INCOMPATIBLE_VIDEO_PARAM are warning status
    {
#if defined(MFX_ENABLE_LOG_UTILITY)
        MFX_LOG(LEVEL_WARN, fileName, lineNum, "%s: returns %s\n", funcName, stsString.c_str());
#endif
        mfxSts = "[warning]  mfxRes = ";
    }

    else if (sts < MFX_ERR_NONE)
    {
#if defined(MFX_ENABLE_LOG_UTILITY)
        MFX_LOG(LEVEL_ERROR, fileName, lineNum, "%s: returns %s\n", funcName, stsString.c_str());
#endif
        mfxSts = "[critical]  mfxRes = ";
    }
    if(sts != MFX_ERR_NONE)
    {
       MFX_LTRACE((&_trace_static_handle, fileName, lineNum, funcName, MFX_TRACE_CATEGORY, MFX_TRACE_LEVEL_INTERNAL, mfxSts.c_str(), MFX_TRACE_FORMAT_S, stsString.c_str()));
    }
    return sts;
}

#define MFX_STS_TRACE(sts) mfx_sts_trace(__FILE__, __LINE__, __FUNCTION__, sts)

#define MFX_SUCCEEDED(sts)          (MFX_STS_TRACE(sts) == MFX_ERR_NONE)
#define MFX_FAILED(sts)             (MFX_STS_TRACE(sts) != MFX_ERR_NONE)
#define MFX_RETURN(sts)             { return MFX_STS_TRACE(sts); }
#define MFX_RETURN_IF_ERR_NONE(sts) { if (MFX_SUCCEEDED(sts)) return MFX_ERR_NONE; }
#define MFX_CHECK(EXPR, ERR)        { if (!(EXPR)) MFX_RETURN(ERR); }

#define MFX_CHECK_NO_RET(EXPR, STS, ERR){ if (!(EXPR)) { std::ignore = MFX_STS_TRACE(ERR); STS = ERR; } }

#define MFX_CHECK_STS(sts)              MFX_CHECK(MFX_SUCCEEDED(sts), sts)
#define MFX_CHECK_STS_RET(sts, ret)     { if (MFX_FAILED(sts)) return ret; }
#define MFX_CHECK_STS_RET_NULL(sts)     MFX_CHECK_STS_RET(sts, nullptr)
#define MFX_SAFE_CALL(FUNC)             { mfxStatus _sts = FUNC; MFX_CHECK_STS(_sts); }
#define MFX_CHECK_NULL_PTR1(pointer)    MFX_CHECK(pointer, MFX_ERR_NULL_PTR)
#define MFX_CHECK_NULL_PTR2(p1, p2)     { MFX_CHECK(p1, MFX_ERR_NULL_PTR); MFX_CHECK(p2, MFX_ERR_NULL_PTR); }
#define MFX_CHECK_NULL_PTR3(p1, p2, p3) { MFX_CHECK(p1, MFX_ERR_NULL_PTR); MFX_CHECK(p2, MFX_ERR_NULL_PTR); MFX_CHECK(p3, MFX_ERR_NULL_PTR); }
#define MFX_CHECK_STS_ALLOC(pointer)    MFX_CHECK(pointer, MFX_ERR_MEMORY_ALLOC)
#define MFX_CHECK_COND(cond)            MFX_CHECK(cond, MFX_ERR_UNSUPPORTED)
#define MFX_CHECK_INIT(InitFlag)        MFX_CHECK(InitFlag, MFX_ERR_MORE_DATA)
#define MFX_CHECK_HDL(hdl)              MFX_CHECK(hdl,      MFX_ERR_INVALID_HANDLE)

#define MFX_CHECK_UMC_ALLOC(err)     { if (err != true) {return MFX_ERR_MEMORY_ALLOC;} }
#define MFX_CHECK_EXBUF_INDEX(index) { if (index == -1) {return MFX_ERR_MEMORY_ALLOC;} }

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR,ERR); }
#define MFX_CHECK_WITH_THROW(EXPR, ERR, EXP)  { if (!(EXPR)) { std::ignore = MFX_STS_TRACE(ERR); throw EXP; } }
#define MFX_CHECK_WITH_THROW_STS(EXPR, ERR) MFX_CHECK_WITH_THROW(EXPR, ERR, std::system_error(mfx::make_error_code(mfxStatus(ERR))))

static const mfxU32 MFX_TIME_STAMP_FREQUENCY = 90000; // will go to mfxdefs.h
static const mfxU64 MFX_TIME_STAMP_INVALID = (mfxU64)-1; // will go to mfxdefs.h
static const mfxU32 NO_INDEX = 0xffffffff;
static const mfxU8  NO_INDEX_U8 = 0xff;
static const mfxU16 NO_INDEX_U16 = 0xffff;
#define MFX_CHECK_UMC_STS(err)  { if (err != static_cast<int>(UMC::UMC_OK)) {return ConvertStatusUmc2Mfx(err);} }

inline
mfxStatus ConvertStatusUmc2Mfx(UMC::Status umcStatus)
{
    switch (umcStatus)
    {
    case UMC::UMC_OK:                    return MFX_ERR_NONE;
    case UMC::UMC_ERR_NULL_PTR:          return MFX_ERR_NULL_PTR;
    case UMC::UMC_ERR_UNSUPPORTED:       return MFX_ERR_UNSUPPORTED;
    case UMC::UMC_ERR_ALLOC:             return MFX_ERR_MEMORY_ALLOC;
    case UMC::UMC_ERR_LOCK:              return MFX_ERR_LOCK_MEMORY;
    case UMC::UMC_ERR_NOT_IMPLEMENTED:   return MFX_ERR_NOT_IMPLEMENTED;
    case UMC::UMC_ERR_GPU_HANG:          return MFX_ERR_GPU_HANG;
    case UMC::UMC_ERR_NOT_ENOUGH_BUFFER: return MFX_ERR_NOT_ENOUGH_BUFFER;
    case UMC::UMC_ERR_NOT_ENOUGH_DATA:   return MFX_ERR_MORE_DATA;
    case UMC::UMC_ERR_SYNC:              return MFX_ERR_MORE_DATA; // need to skip bad frames
    default:                             return MFX_ERR_UNKNOWN;   // need general error code here
    }
}

inline
UMC::Status ConvertStatusMfx2Umc(mfxStatus mfxStatus)
{
    switch (mfxStatus)
    {
    case MFX_ERR_NONE:              return UMC::UMC_OK;
    case MFX_ERR_NULL_PTR:          return UMC::UMC_ERR_NULL_PTR;
    case MFX_ERR_UNSUPPORTED:       return UMC::UMC_ERR_UNSUPPORTED;
    case MFX_ERR_MEMORY_ALLOC:      return UMC::UMC_ERR_ALLOC;
    case MFX_ERR_LOCK_MEMORY:       return UMC::UMC_ERR_LOCK;
    case MFX_ERR_NOT_ENOUGH_BUFFER: return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
    case MFX_ERR_MORE_DATA:         return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    case MFX_ERR_NOT_IMPLEMENTED:   return UMC::UMC_ERR_NOT_IMPLEMENTED;
    case MFX_ERR_GPU_HANG:          return UMC::UMC_ERR_GPU_HANG;
    case MFX_ERR_UNKNOWN:
    default:
        return UMC::UMC_ERR_FAILED;
    }
}

inline
mfxF64 GetUmcTimeStamp(mfxU64 ts)
{
    return ts == MFX_TIME_STAMP_INVALID ? -1.0 : ts / (mfxF64)MFX_TIME_STAMP_FREQUENCY;
}

inline
mfxU64 GetMfxTimeStamp(mfxF64 ts)
{
    return ts < 0.0 ? MFX_TIME_STAMP_INVALID : (mfxU64)(ts * MFX_TIME_STAMP_FREQUENCY + .5);
}

inline
bool LumaIsNull(const mfxFrameSurface1 * surf)
{
    if (surf->Info.FourCC == MFX_FOURCC_Y410)
    {
        return !surf->Data.Y410;
    }
    else
    {
        return !surf->Data.Y;
    }
}

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(PTR)   { if (PTR) { PTR->Release(); PTR = NULL; } }
#endif

#define IS_PROTECTION_ANY(val) (false)

#define MFX_COPY_FIELD_NO_LOG(Field)       buf_dst.Field = buf_src.Field

#if !defined(MFX_ENABLE_LOG_UTILITY)
#define MFX_COPY_FIELD(Field)       buf_dst.Field = buf_src.Field
#define MFX_COPY_ARRAY_FIELD(Array) std::copy(std::begin(buf_src.Array), std::end(buf_src.Array), std::begin(buf_dst.Array))
#else
#define MFX_COPY_FIELD(Field)                                                                   \
    buf_dst.Field = buf_src.Field;                                                              \
    {                                                                                           \
        const std::string fieldForamt = GetNumberFormat(buf_src.Field);                         \
        const std::string typeName    = std::string(typeid(buf_src).name());                    \
        const std::string format      = typeName + ".%s = " + fieldForamt + "\n";               \
        MFX_LOG_API_TRACE(format.c_str(), #Field, buf_src.Field);                               \
    }

#define MFX_COPY_ARRAY_FIELD(Array)                                                             \
    std::copy(std::begin(buf_src.Array), std::end(buf_src.Array), std::begin(buf_dst.Array));   \
    {                                                                                           \
        int idx = 0;                                                                            \
        const std::string fieldForamt = GetNumberFormat(buf_src.Array[0]);                      \
        const std::string typeName    = std::string(typeid(buf_src).name());                    \
        const std::string format      = typeName + ".%s[%d] = " + fieldForamt + "\n";           \
        for (auto it = std::begin(buf_src.Array); it != std::end(buf_src.Array); it++, idx++) { \
            MFX_LOG_API_TRACE(format.c_str(), #Array, idx, *it);                                \
        }                                                                                       \
    }
#endif

namespace mfx
{
template<typename T>
T GetEnv(const char* name, T defaultVal)
{
#if defined (MFX_ENV_CFG_ENABLE)
    if (const char* strVal = std::getenv(name))
    {
        std::istringstream(strVal) >> defaultVal;
        MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL, name, "=%s", strVal);

        return defaultVal;
    }
#endif
#if defined (MFX_TRACE_ENABLE)
    {
        std::ostringstream ss;
        ss << name << "=" << defaultVal;
        MFX_LTRACE_MSG(MFX_TRACE_LEVEL_INTERNAL, ss.str().c_str());
    }
#endif
    return defaultVal;
}

// switch to std::clamp when C++17 support will be enabled
// Clip value v to range [lo, hi]
template<class T>
constexpr const T& clamp( const T& v, const T& lo, const T& hi )
{
    return std::min(hi, std::max(v, lo));
}

// Comp is comparison function object with meaning of 'less' operator (i.e. std::less<> or operator<)
template<class T, class Compare>
constexpr const T& clamp( const T& v, const T& lo, const T& hi, Compare comp )
{
    return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
}

// Clip value to range [0, 255]
template<class T>
constexpr uint8_t byte_clamp(T v)
{
    return uint8_t(clamp<T>(v, 0, 255));
}

// Aligns value to next power of two
template<class T> inline
T align2_value(T value, size_t alignment = 16)
{
    assert((alignment & (alignment - 1)) == 0);
    return static_cast<T> ((value + (alignment - 1)) & ~(alignment - 1));
}

template <class T>
constexpr size_t size(const T& c)
{
    return (size_t)c.size();
}

template <class T, size_t N>
constexpr size_t size(const T(&)[N])
{
    return N;
}

template<class T>
constexpr T CeilDiv(T x, T y)
{
    return (x + y - 1) / y;
}

inline mfxU32 CeilLog2(mfxU32 x)
{
    mfxU32 l = 0;
    while (x > (1U << l))
        ++l;
    return l;
}

template <class F>
struct TupleArgs;

template <typename TRes, typename... TArgs>
struct TupleArgs<TRes(TArgs...)>
{
    using type = std::tuple<TArgs...>;
};
template <typename TRes, typename... TArgs>
struct TupleArgs<TRes(*)(TArgs...)>
{
    using type = std::tuple<TArgs...>;
};

template <class F>
struct result_of;

template <typename TRes, typename... TArgs>
struct result_of<TRes(TArgs...)> : std::result_of<TRes(TArgs...)> {};

template <typename TRes, typename... TArgs>
struct result_of<TRes(*const&)(TArgs...)>
{
    using type = TRes;
};

template<typename TFunc, typename TTuple, size_t ...S >
inline typename mfx::result_of<TFunc>::type
    apply_impl(TFunc&& fn, TTuple&& t, std::index_sequence<S...>)
{
    return fn(std::get<S>(t) ...);
}

template<typename TFunc, typename TTuple>
inline typename mfx::result_of<TFunc>::type
    apply(TFunc&& fn, TTuple&& t)
{
    return apply_impl(
        std::forward<TFunc>(fn)
        , std::forward<TTuple>(t)
        , typename std::make_index_sequence<std::tuple_size<typename std::remove_reference<TTuple>::type>::value>());
}

template<class T>
class IterStepWrapper
    : public std::iterator_traits<T>
{
public:
    using iterator_category = std::forward_iterator_tag;
    using iterator_type = IterStepWrapper;
    using reference = typename std::iterator_traits<T>::reference;
    using pointer = typename std::iterator_traits<T>::pointer;

    IterStepWrapper(T ptr, ptrdiff_t step = 1)
        : m_ptr(ptr)
        , m_step(step)
    {}
    iterator_type& operator++()
    {
        std::advance(m_ptr, m_step);
        return *this;
    }
    iterator_type operator++(int)
    {
        auto i = *this;
        ++(*this);
        return i;
    }
    reference operator*() { return *m_ptr; }
    pointer operator->() { return m_ptr; }
    bool operator==(const iterator_type& other)
    {
        return
            m_ptr == other.m_ptr
            || abs(std::distance(m_ptr, other.m_ptr)) < std::max(abs(m_step), abs(other.m_step));
    }
    bool operator!=(const iterator_type& other)
    {
        return !((*this) == other);
    }
private:
    T m_ptr;
    ptrdiff_t m_step;
};

template <class T>
inline IterStepWrapper<T> MakeStepIter(T ptr, ptrdiff_t step = 1)
{
    return IterStepWrapper<T>(ptr, step);
}

class OnExit
    : public std::function<void()>
{
public:
    OnExit(const OnExit&) = delete;

    template<class... TArg>
    OnExit(TArg&& ...arg)
        : std::function<void()>(std::forward<TArg>(arg)...)
    {}

    ~OnExit()
    {
        if (operator bool())
            operator()();
    }

    template<class... TArg>
    OnExit& operator=(TArg&& ...arg)
    {
        std::function<void()> tmp(std::forward<TArg>(arg)...);
        swap(tmp);
        return *this;
    }
};

namespace options //MSDK API options verification utilities
{
    //Each Check... function return true if verification failed, false otherwise
    template <class T>
    inline bool Check(const T&)
    {
        return true;
    }

    template <class T, T val, T... other>
    inline bool Check(const T & opt)
    {
        if (opt == val)
            return false;
        return Check<T, other...>(opt);
    }

    template <class T, T val>
    inline bool CheckGE(T opt)
    {
        return !(opt >= val);
    }

    template <class T, class... U>
    inline bool Check(T & opt, T next, U... other)
    {
        if (opt == next)
            return false;
        return Check(opt, other...);
    }

    template <class T>
    inline bool CheckOrZero(T& opt)
    {
        opt = T(0);
        return true;
    }

    template <class T, T val, T... other>
    inline bool CheckOrZero(T & opt)
    {
        if (opt == val)
            return false;
        return CheckOrZero<T, other...>(opt);
    }

    template <class T, class... U>
    inline bool CheckOrZero(T & opt, T next, U... other)
    {
        if (opt == next)
            return false;
        return CheckOrZero(opt, (T)other...);
    }

    template <class T>
    inline bool CheckOrSetDefault(T& opt, T dflt)
    {
        opt = T(dflt);
        return true;
    }

    template <class T, T val, T... other>
    inline bool CheckOrSetDefault(T & opt, T dflt)
    {
        if (opt == val)
            return false;
        return CheckOrSetDefault<T, other...>(opt, dflt);
    }

    template <class T, class U>
    inline bool CheckMaxOrZero(T & opt, U max)
    {
        if (opt <= max)
            return false;
        opt = 0;
        return true;
    }

    template <class T, class U>
    inline bool CheckMinOrZero(T & opt, U min)
    {
        if (opt >= min)
            return false;
        opt = 0;
        return true;
    }

    template <class T, class U>
    inline bool CheckMaxOrClip(T & opt, U max)
    {
        if (opt <= max)
            return false;
        opt = T(max);
        return true;
    }

    template <class T, class U>
    inline bool CheckMinOrClip(T & opt, U min)
    {
        if (opt >= min)
            return false;
        opt = T(min);
        return true;
    }

    template<class T, class I>
    inline bool CheckRangeOrClip(T & opt, I min, I max)
    {
        if (opt < static_cast<T>(min))
        {
            opt = static_cast<T>(min);
            return true;
        }

        if (opt > static_cast<T>(max))
        {
            opt = static_cast<T>(max);
            return true;
        }

        return false;
    }

    template <class T>
    inline bool CheckRangeOrSetDefault(T & opt, T min, T max, T dflt)
    {
        if (opt >= min && opt <= max)
            return false;
        opt = dflt;
        return true;
    }

    inline bool CheckTriState(mfxU16 opt)
    {
        return Check<mfxU16
            , MFX_CODINGOPTION_UNKNOWN
            , MFX_CODINGOPTION_ON
            , MFX_CODINGOPTION_OFF>(opt);
    }

    inline bool CheckTriStateOrZero(mfxU16& opt)
    {
        return CheckOrZero<mfxU16
            , MFX_CODINGOPTION_UNKNOWN
            , MFX_CODINGOPTION_ON
            , MFX_CODINGOPTION_OFF>(opt);
    }

    template<class TVal, class TArg, typename std::enable_if<!std::is_constructible<TVal, TArg>::value, int>::type = 0>
    inline TVal GetOrCall(TArg val) { return val(); }

    template<class TVal, class TArg, typename = typename std::enable_if<std::is_constructible<TVal, TArg>::value>::type>
    inline TVal GetOrCall(TArg val) { return TVal(val); }

    template<typename T, typename TF>
    inline bool SetDefault(T& opt, TF get_dflt)
    {
        if (opt)
            return false;
        opt = GetOrCall<T>(get_dflt);
        return true;
    }

    inline void SetDefaultOpt(mfxU16 &opt, bool bCond)
    {
        SetDefault(opt, bCond ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
    }

    template<typename T, typename TF>
    inline bool SetIf(T& opt, bool bSet, TF get)
    {
        if (!bSet)
            return false;
        opt = GetOrCall<T>(get);
        return true;
    }

    template<class T, class TF, class... TA>
    inline bool SetIf(T& opt, bool bSet, TF&& get, TA&&... arg)
    {
        if (!bSet)
            return false;
        opt = get(std::forward<TA>(arg)...);
        return true;
    }

    template <class T>
    inline bool InheritOption(T optInit, T & optReset)
    {
        if (optReset == 0)
        {
            optReset = optInit;
            return true;
        }
        return false;
    }

    template<class TSrcIt, class TDstIt>
    TDstIt InheritOptions(TSrcIt initFirst, TSrcIt initLast, TDstIt resetFirst)
    {
        while (initFirst != initLast)
        {
            InheritOption(*initFirst++, *resetFirst++);
        }
        return resetFirst;
    }

    inline mfxU16 Bool2CO(bool bOptON)
    {
        return mfxU16(MFX_CODINGOPTION_OFF - !!bOptON * MFX_CODINGOPTION_ON);
    }

    template<class T>
    inline bool AlignDown(T& value, mfxU32 alignment)
    {
        assert((alignment & (alignment - 1)) == 0); // should be 2^n
        if (!(value & (alignment - 1))) return false;
        value = value & ~(alignment - 1);
        return true;
    }

    template<class T>
    inline bool AlignUp(T& value, mfxU32 alignment)
    {
        assert((alignment & (alignment - 1)) == 0); // should be 2^n
        if (!(value & (alignment - 1))) return false;
        value = (value + alignment - 1) & ~(alignment - 1);
        return true;
    }

    namespace frametype
    {
        inline bool IsIdr(mfxU32 type)
        {
            return !!(type & MFX_FRAMETYPE_IDR);
        }
        inline bool IsI(mfxU32 type)
        {
            return !!(type & MFX_FRAMETYPE_I);
        }
        inline bool IsB(mfxU32 type)
        {
            return !!(type & MFX_FRAMETYPE_B);
        }
        inline bool IsP(mfxU32 type)
        {
            return !!(type & MFX_FRAMETYPE_P);
        }
        inline bool IsRef(mfxU32 type)
        {
            return !!(type & MFX_FRAMETYPE_REF);
        }
    }
}

class PODArraysHolder
{
public:
    template<typename T>
    T& PushBack(T*& p)
    {
        auto IsSameData = [p](std::vector<uint8_t>& v) { return (uint8_t*)p == v.data(); };
        auto it = std::find_if(std::begin(m_attachedData), std::end(m_attachedData), IsSameData);

        if (it == m_attachedData.end())
        {
            m_attachedData.emplace_back(std::vector<uint8_t>(sizeof(T), 0));
            return *(p = (T*)m_attachedData.back().data());
        }

        auto itNew = it->insert(it->end(), sizeof(T), 0);
        p = (T*)it->data();

        return *(T*)&*itNew;
    }
protected:
    std::list<std::vector<uint8_t>> m_attachedData;
};

template <class Duration, class Representation>
class Timer
{
public:

    Timer(Representation left)
        : m_end(std::chrono::steady_clock::now() + Duration(left))
    {}

    Timer(std::chrono::steady_clock::time_point end)
        : m_end(end)
    {}

    Representation Left() const
    {
        auto now = std::chrono::steady_clock::now();
        return m_end < now ?
            Representation{} :
            Representation(std::chrono::duration_cast<Duration>(m_end - now).count());
    }

    std::chrono::steady_clock::time_point End() const
    {
        return m_end;
    }

    static
        bool Expired(Representation left)
    {
        return left == Representation{};
    }

    bool Expired() const
    {
        return Expired(Left());
    }

protected:
    std::chrono::steady_clock::time_point m_end;
};

template <class Duration, class Representation>
class ResettableTimer : public Timer<Duration, Representation>
{
public:
    ResettableTimer(Representation left = Representation{})
        : Timer<Duration, Representation>(left)
    {}

    void Reset(Representation left)
    {
        this->m_end = std::chrono::steady_clock::now() + Duration(left);

        m_running = true;
    }

    bool IsRunnig() const
    {
        return m_running;
    }

    void Stop()
    {
        m_running = false;
    }

private:
    bool m_running = false;
};

template <typename Representation>
using TimerMs = Timer<std::chrono::milliseconds, Representation>;

using ResettableTimerMs = ResettableTimer<std::chrono::milliseconds, std::chrono::milliseconds>;

inline mfxExtBuffer* GetExtBuffer(mfxExtBuffer** ExtParam, mfxU32 NumExtParam, mfxU32 BufferId, mfxU32 offset = 0)
{
    if (!ExtParam)
        return nullptr;

    mfxU32 count = 0;
    for (mfxU32 i = 0; i < NumExtParam; ++i)
    {
        if (ExtParam[i] && ExtParam[i]->BufferId == BufferId && count++ == offset)
        {
            return ExtParam[i];
        }
    }

    return nullptr;
}
using mfx_shared_lib_path_string = std::string;

inline mfxHDL shared_lib_load(const mfx_shared_lib_path_string& shared_lib_file_name)
{

    if (shared_lib_file_name.empty())
        return nullptr;

    return dlopen(shared_lib_file_name.c_str(), RTLD_LAZY);
}

inline mfxHDL shared_lib_get_addr(mfxHDL shared_lib_handle, const std::string & shared_lib_func_name)
{

    if (!shared_lib_handle)
        return nullptr;

    return dlsym(shared_lib_handle, shared_lib_func_name.c_str());
}

inline void shared_lib_free(mfxHDL shared_lib_handle)
{

    if (!shared_lib_handle)
        return;

    std::ignore = dlclose(shared_lib_handle);
}

class mfx_shared_lib_holder
{
public:

    mfx_shared_lib_holder(const mfx_shared_lib_path_string& path, const char** functions_to_load, size_t n_functions_to_load)
        : m_handle(shared_lib_load(path))
    {
        MFX_CHECK_WITH_THROW_STS(m_handle, MFX_ERR_INVALID_HANDLE);

        // We can optionally load some functions from shared library
        if (!n_functions_to_load)
            return;

        MFX_CHECK_WITH_THROW_STS(functions_to_load, MFX_ERR_INVALID_HANDLE);

        for (size_t idx = 0; idx < n_functions_to_load; ++idx)
        {
            mfxHDL loaded_hdl = mfx::shared_lib_get_addr(m_handle, functions_to_load[idx]);
            MFX_CHECK_WITH_THROW_STS(loaded_hdl, MFX_ERR_INVALID_HANDLE);

            m_loaded_functions[functions_to_load[idx]] = loaded_hdl;
        }
    }

    virtual ~mfx_shared_lib_holder()
    {
        shared_lib_free(m_handle);
    }

    mfxHDL get_function_hdl(const std::string& fname)
    {
        auto it = m_loaded_functions.find(fname);

        return it != m_loaded_functions.end() ? it->second : nullptr;
    }

protected:
    mfxHDL m_handle = nullptr;

    std::map<std::string, mfxHDL> m_loaded_functions;

};

inline mfxU32 GetNumDataPlanesFromFourcc(mfxU32 fourcc)
{

    switch (fourcc)
    {
#if defined (MFX_ENABLE_FOURCC_RGB565)
    case MFX_FOURCC_RGB565:
#endif
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_BGR4:
    case MFX_FOURCC_A2RGB10:
    case MFX_FOURCC_ARGB16:
    case MFX_FOURCC_ABGR16:
    case MFX_FOURCC_R16:
    case MFX_FOURCC_P8:
    case MFX_FOURCC_P8_TEXTURE:
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_AYUV_RGB4:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_Y416:
    case MFX_FOURCC_ABGR16F:
    case MFX_FOURCC_XYUV:
        return 1u;

    case MFX_FOURCC_NV12:
    case MFX_FOURCC_NV16:
    case MFX_FOURCC_NV21:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_P016:
    case MFX_FOURCC_P210:
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_UYVY:
        return 2u;

    case MFX_FOURCC_YV12:
    case MFX_FOURCC_IYUV:
    case MFX_FOURCC_I010:
    case MFX_FOURCC_I210:
    case MFX_FOURCC_I422:
#ifdef MFX_ENABLE_RGBP
    case MFX_FOURCC_RGBP:
#endif
    case MFX_FOURCC_BGRP:
        return 3u;

    default:
        return 0u;
    }
}

} //namespace mfx

inline mfxStatus CheckAndDestroyVAbuffer(VADisplay display, VABufferID & buffer_id)
{
    if (buffer_id != VA_INVALID_ID)
    {
        VAStatus vaSts = vaDestroyBuffer(display, buffer_id);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        buffer_id = VA_INVALID_ID;
    }

    return MFX_ERR_NONE;
}

inline mfxStatus AddRefSurface(mfxFrameSurface1 & surf, bool allow_legacy_surface = false)
{
    if (allow_legacy_surface && !surf.FrameInterface) { return MFX_ERR_NONE; }

    MFX_CHECK(surf.FrameInterface && surf.FrameInterface->AddRef, MFX_ERR_UNSUPPORTED);

    return surf.FrameInterface->AddRef(&surf);
}

inline mfxStatus ReleaseSurface(mfxFrameSurface1 & surf, bool allow_legacy_surface = false)
{
    if (allow_legacy_surface && !surf.FrameInterface) { return MFX_ERR_NONE; }

    MFX_CHECK(surf.FrameInterface && surf.FrameInterface->Release, MFX_ERR_UNSUPPORTED);

    return surf.FrameInterface->Release(&surf);
}

struct surface_refcount_scoped_lock : public std::unique_ptr<mfxFrameSurface1, void(*)(mfxFrameSurface1* surface)>
{
    surface_refcount_scoped_lock(mfxFrameSurface1* surface)
        : std::unique_ptr<mfxFrameSurface1, void(*)(mfxFrameSurface1* surface)>(
            surface, [](mfxFrameSurface1* surface)
    {
        std::ignore = MFX_STS_TRACE(ReleaseSurface(*surface));
    })
    {}
};

struct mfxRefCountable
{
    virtual mfxU32    GetRefCounter() const = 0;
    virtual void      AddRef()              = 0;
    virtual mfxStatus Release()             = 0;
};

template <typename T>
struct mfxRefCountableInstance
{
    static mfxRefCountable* Get(T* object)
    { return static_cast<mfxRefCountable*>(object); }
};

template <typename T, typename U = T>
class mfxRefCountableImpl
    : public mfxRefCountable
    , public T
{
protected:
    template <typename X, typename = void>
    struct HasRefInterface : std::false_type { };

    template <typename X>
    struct HasRefInterface <X, decltype(X::RefInterface, void())> : std::true_type { };

    template <typename X, typename std::enable_if<HasRefInterface<X>::value, bool>::type = true>
    void AssignFunctionPointers()
    {
        T::RefInterface.AddRef        = _AddRef2;
        T::RefInterface.Release       = _Release2;
        T::RefInterface.GetRefCounter = _GetRefCounter2;
    }

    template <typename X, typename std::enable_if<!HasRefInterface<X>::value, bool>::type = true>
    void AssignFunctionPointers()
    {
        T::AddRef        = _AddRef;
        T::Release       = _Release;
        T::GetRefCounter = _GetRefCounter;
    }
public:
    mfxRefCountableImpl()
        : T()
        , m_ref_count(0)
    {
#if defined (MFX_DEBUG_REFCOUNT)
        g_global_registry.RegisterRefcountObject(this);
#endif

        AssignFunctionPointers<T>();
    }

    virtual ~mfxRefCountableImpl()
    {
        if (m_ref_count.load(std::memory_order_relaxed) != 0)
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_UNKNOWN);
        }
#if defined (MFX_DEBUG_REFCOUNT)
        g_global_registry.UnregisterRefcountObject(this);
#endif
    }

    mfxU32 GetRefCounter() const override
    {
        return m_ref_count.load(std::memory_order_relaxed);
    }

    void AddRef() override
    {
        std::ignore = m_ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    mfxStatus Release() override
    {
        MFX_CHECK(m_ref_count.load(std::memory_order_relaxed), MFX_ERR_UNDEFINED_BEHAVIOR);

        // fetch_sub return value immediately preceding
        if (m_ref_count.fetch_sub(1, std::memory_order_relaxed) - 1 == 0)
        {
            // Refcount is zero

            // Update state of parent allocator if set
            Close();

            // Delete refcounted object, here wrapper is finally destroyed and underlying texture / memory released
            delete this;
        }

        return MFX_ERR_NONE;
    }

    static mfxStatus _AddRef(U* object)
    {
        MFX_CHECK_NULL_PTR1(object);
        auto instance = mfxRefCountableInstance<U>::Get(object);
        MFX_CHECK_HDL(instance);

        instance->AddRef();
        return MFX_ERR_NONE;
    }

    static mfxStatus _Release(U* object)
    {
        MFX_CHECK_NULL_PTR1(object);
        auto instance = mfxRefCountableInstance<U>::Get(object);
        MFX_CHECK_HDL(instance);

        return instance->Release();
    }

    static mfxStatus _GetRefCounter(U* object, mfxU32* counter)
    {
        MFX_CHECK_NULL_PTR1(object);
        MFX_CHECK_NULL_PTR1(counter);

        auto instance = mfxRefCountableInstance<U>::Get(object);
        MFX_CHECK_HDL(instance);

        *counter = instance->GetRefCounter();
        return MFX_ERR_NONE;
    }

    static mfxStatus _AddRef2(mfxRefInterface* object)
    {
        MFX_CHECK_NULL_PTR1(object);
        auto instance = static_cast<mfxRefCountable*>(object->Context);
        MFX_CHECK_HDL(instance);

        instance->AddRef();
        return MFX_ERR_NONE;
    }

    static mfxStatus _Release2(mfxRefInterface* object)
    {
        MFX_CHECK_NULL_PTR1(object);
        auto instance = static_cast<mfxRefCountable*>(object->Context);
        MFX_CHECK_HDL(instance);

        return instance->Release();
    }

    static mfxStatus _GetRefCounter2(mfxRefInterface* object, mfxU32* counter)
    {
        MFX_CHECK_NULL_PTR1(object);
        MFX_CHECK_NULL_PTR1(counter);

        auto instance = static_cast<mfxRefCountable*>(object->Context);
        MFX_CHECK_HDL(instance);

        *counter = instance->GetRefCounter();
        return MFX_ERR_NONE;
    }

protected:

    virtual void Close() { return; }

private:

    std::atomic<uint32_t> m_ref_count;
};

// This class calls release at destruction
template <typename RefCountable>
struct unique_ptr_refcountable : public std::unique_ptr<RefCountable, void(*)(RefCountable* obj)>
{
    explicit unique_ptr_refcountable(RefCountable* refcountable = nullptr)
        : std::unique_ptr<RefCountable, void(*)(RefCountable* obj)>(
            refcountable, [](RefCountable* obj)
            {
                auto instance = mfxRefCountableInstance<RefCountable>::Get(obj);
                instance->Release();
            })
    {}
};

#define MFX_EQ_FIELD(Field) l.Field == r.Field
#define MFX_EQ_ARRAY(Array, Num) std::equal(l.Array, l.Array + Num, r.Array)

#define MFX_DECL_OPERATOR_NOT_EQ(Name)                      \
static inline bool operator!=(Name const& l, Name const& r) \
{                                                           \
    return !(l == r);                                       \
}

static inline bool operator==(mfxPluginUID const& l, mfxPluginUID const& r)
{
    return MFX_EQ_ARRAY(Data, 16);
}

MFX_DECL_OPERATOR_NOT_EQ(mfxPluginUID)

static inline bool operator==(mfxGUID const& l, mfxGUID const& r)
{
    return MFX_EQ_ARRAY(Data, 16);
}

MFX_DECL_OPERATOR_NOT_EQ(mfxGUID)

static inline bool operator==(mfxExtBuffer const& l, mfxExtBuffer const& r)
{
    return MFX_EQ_FIELD(BufferId) && MFX_EQ_FIELD(BufferSz);
}

static inline bool operator==(mfxExtAllocationHints const& l, mfxExtAllocationHints const& r)
{
    return MFX_EQ_FIELD(Header) && MFX_EQ_FIELD(AllocationPolicy) && MFX_EQ_FIELD(NumberToPreAllocate) && MFX_EQ_FIELD(DeltaToAllocateOnTheFly) && MFX_EQ_FIELD(VPPPoolType) && MFX_EQ_FIELD(Wait);
}

inline mfxStatus CheckAllocationHintsBuffer(const mfxExtAllocationHints& allocation_hints, bool is_vpp = false)
{
    switch (allocation_hints.AllocationPolicy)
    {
    case MFX_ALLOCATION_OPTIMAL:
        MFX_CHECK(!allocation_hints.NumberToPreAllocate, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(!allocation_hints.DeltaToAllocateOnTheFly, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        break;
    case MFX_ALLOCATION_UNLIMITED:
        MFX_CHECK(!allocation_hints.DeltaToAllocateOnTheFly, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        break;
    case MFX_ALLOCATION_LIMITED:
        break;

    default:
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    if (is_vpp)
    {
        MFX_CHECK(allocation_hints.VPPPoolType == MFX_VPP_POOL_IN || allocation_hints.VPPPoolType == MFX_VPP_POOL_OUT, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

enum class ComponentType
{
    UNINITIALIZED = 0,
    DECODE        = 1,
    ENCODE        = 2,
    VPP           = 3
};

template <typename CacheType>
struct surface_cache_controller
{
    surface_cache_controller(CacheType* cache, ComponentType type, mfxVPPPoolType vpp_pool_type = mfxVPPPoolType(-1))
        : m_cache(cache)
        , m_type(type)
        , m_vpp_pool(vpp_pool_type)
    {}

    ~surface_cache_controller()
    {
        Close();
    }

    mfxStatus SetupCache(mfxSession session, const mfxVideoParam& par)
    {
        auto it = std::find_if(par.ExtParam, par.ExtParam + par.NumExtParam,
            [this](mfxExtBuffer* buffer)
            {
                return buffer->BufferId == MFX_EXTBUFF_ALLOCATION_HINTS &&
                    (m_type != ComponentType::VPP || reinterpret_cast<mfxExtAllocationHints*>(buffer)->VPPPoolType == m_vpp_pool);
            });

        mfxExtAllocationHints* allocation_hints = nullptr;

        bool optimal_caching_policy_requested = false;

        if (it != par.ExtParam + par.NumExtParam)
        {
            allocation_hints = reinterpret_cast<mfxExtAllocationHints*>(*it);
            optimal_caching_policy_requested = allocation_hints->AllocationPolicy == MFX_ALLOCATION_OPTIMAL;
        }

        if (optimal_caching_policy_requested)
        {
            // Use temporal copy for possible IOPattern adjustment. Hitorically Init doesn't require setting of IOPatter, but QueryIOSurf does.
            // This function is invoked during Init
            mfxVideoParam tmp_par = par;

            switch (m_type)
            {
            case ComponentType::DECODE:
            {
                mfxFrameAllocRequest req = {};

                if (!tmp_par.IOPattern)
                    tmp_par.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

                mfxStatus sts = MFX_STS_TRACE(MFXVideoDECODE_QueryIOSurf(session, &tmp_par, &req));
                MFX_CHECK(sts >= MFX_ERR_NONE, sts);

                m_required_num_surf = req.NumFrameSuggested;
            }
            break;

            case ComponentType::ENCODE:
            {
                mfxFrameAllocRequest req = {};

                if (!tmp_par.IOPattern)
                    tmp_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

                mfxStatus sts = MFX_STS_TRACE(MFXVideoENCODE_QueryIOSurf(session, &tmp_par, &req));
                MFX_CHECK(sts >= MFX_ERR_NONE, sts);

                m_required_num_surf = req.NumFrameSuggested;
            }
            break;

            case ComponentType::VPP:
            {
                mfxFrameAllocRequest req[2] = {};

                if (!tmp_par.IOPattern)
                    tmp_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

                mfxStatus sts = MFX_STS_TRACE(MFXVideoVPP_QueryIOSurf(session, &tmp_par, req));
                MFX_CHECK(sts >= MFX_ERR_NONE, sts);

                MFX_CHECK(m_vpp_pool == MFX_VPP_POOL_IN || m_vpp_pool == MFX_VPP_POOL_OUT, MFX_ERR_INVALID_VIDEO_PARAM);

                m_required_num_surf = req[m_vpp_pool].NumFrameSuggested;
            }
            break;

            default:
                MFX_RETURN(MFX_ERR_NOT_IMPLEMENTED);
            }
        }

        MFX_CHECK(m_cache, MFX_ERR_NOT_INITIALIZED);

        if (!allocation_hints)
            return MFX_ERR_NONE;

        MFX_SAFE_CALL(CheckAllocationHintsBuffer(*allocation_hints, m_type == ComponentType::VPP));
        MFX_SAFE_CALL(m_cache->SetupPolicy(*allocation_hints));

        if (optimal_caching_policy_requested)
        {
            unique_ptr_refcountable<mfxSurfacePoolInterface> scoped_surface_lock(m_cache.get());
            // AddRef here because m_cache is also a smart pointer, so will be decremented twice
            MFX_SAFE_CALL(scoped_surface_lock->AddRef(scoped_surface_lock.get()));

            MFX_CHECK(m_updated_caches.find(scoped_surface_lock) == std::end(m_updated_caches), MFX_ERR_UNKNOWN);

            MFX_SAFE_CALL(m_cache->UpdateLimits(m_required_num_surf));

            m_updated_caches[std::move(scoped_surface_lock)] = m_required_num_surf;
        }

        m_cache_hints_set = *allocation_hints;

        return MFX_ERR_NONE;
    }

    mfxStatus Update(const mfxFrameSurface1& surf)
    {
        if (!surf.FrameInterface)
            return MFX_ERR_NONE;

        MFX_CHECK_HDL(surf.FrameInterface->QueryInterface);

        mfxSurfacePoolInterface* pool_interface = nullptr;
        MFX_SAFE_CALL(surf.FrameInterface->QueryInterface(const_cast<mfxFrameSurface1*>(&surf), MFX_GUID_SURFACE_POOL, reinterpret_cast<mfxHDL*>(&pool_interface)));

        MFX_CHECK_HDL(pool_interface);

        unique_ptr_refcountable<mfxSurfacePoolInterface> scoped_surface_lock(pool_interface);

        // If pool already was updated, do nothing
        if (m_updated_caches.find(scoped_surface_lock) != std::end(m_updated_caches))
            return MFX_ERR_NONE;

        MFX_CHECK_HDL(scoped_surface_lock->GetAllocationPolicy);

        mfxPoolAllocationPolicy policy;
        MFX_SAFE_CALL(scoped_surface_lock->GetAllocationPolicy(scoped_surface_lock.get(), &policy));

        if (policy != MFX_ALLOCATION_OPTIMAL)
            return MFX_ERR_NONE;

        MFX_SAFE_CALL(scoped_surface_lock->SetNumSurfaces(scoped_surface_lock.get(), m_required_num_surf));

        m_updated_caches[std::move(scoped_surface_lock)] = m_required_num_surf;

        return MFX_ERR_NONE;
    }

    mfxStatus ResetCache(const mfxVideoParam& par)
    {
        return ResetCache(par.ExtParam, par.NumExtParam);
    }

    mfxStatus ResetCache(mfxExtBuffer** ExtParam, mfxU16 NumExtParam)
    {
        MFX_CHECK(m_cache, MFX_ERR_NOT_INITIALIZED);

        auto allocation_hints = std::find_if(ExtParam, ExtParam + NumExtParam,
            [this](mfxExtBuffer* buffer)
        {
            return buffer->BufferId == MFX_EXTBUFF_ALLOCATION_HINTS
                /* According to VPL spec reserved fields should be zeroed, so VPPPoolType is zero for non-VPP components */
                && reinterpret_cast<mfxExtAllocationHints*>(buffer)->VPPPoolType == m_cache_hints_set.VPPPoolType;
        });

        if (allocation_hints != ExtParam + NumExtParam)
        {
            // For now it is not allowed to change cache parameters during Reset
            MFX_CHECK(*reinterpret_cast<mfxExtAllocationHints*>(*allocation_hints) == m_cache_hints_set, MFX_ERR_INVALID_VIDEO_PARAM);
        }

        return MFX_ERR_NONE;
    }

    void Close()
    {
        if (!m_cache)
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
            return;
        }

        // Revoke in other caches (pools)
        for (auto& pool_num_surf : m_updated_caches)
            std::ignore = MFX_STS_TRACE(pool_num_surf.first->RevokeSurfaces(pool_num_surf.first.get(), pool_num_surf.second));

        m_updated_caches.clear();
    }

    CacheType* operator->()
    {
        MFX_CHECK_WITH_THROW_STS(m_cache, MFX_ERR_NOT_INITIALIZED);

        return m_cache.get();
    }

    mfxStatus GetSurface(mfxFrameSurface1*& output_surf, mfxSurfaceHeader* import_surface)
    {
        MFX_CHECK(m_cache, MFX_ERR_NOT_INITIALIZED);

        MFX_RETURN(m_cache->GetSurface(output_surf, false, import_surface));
    }

private:
    unique_ptr_refcountable<CacheType>         m_cache;

    ComponentType                              m_type;
    mfxVPPPoolType                             m_vpp_pool;

    mfxU16                                     m_required_num_surf = 0;
    mfxExtAllocationHints                      m_cache_hints_set   = {};

    struct ptr_comparator
    {
        bool operator()(const unique_ptr_refcountable<mfxSurfacePoolInterface>& left, const unique_ptr_refcountable<mfxSurfacePoolInterface>& right) const
        {
            return left.get() < right.get();
        }
    };

    std::map<unique_ptr_refcountable<mfxSurfacePoolInterface>, mfxU32, ptr_comparator> m_updated_caches;
};

struct GUID
{
    size_t GetHashCode() const
    {
        std::stringstream ss;
        ss << Data1 << Data2 << Data3
            // Pass Data4 element-wise to allow zeroes in GUID
            << Data4[0] << Data4[1] << Data4[2] << Data4[3] << Data4[4] << Data4[5] << Data4[6] << Data4[7];
        return std::hash<std::string>()(ss.str());
    }

    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
};

static inline int operator==(const GUID& guidOne, const GUID& guidOther)
{
    return
        guidOne.Data1 == guidOther.Data1 &&
        guidOne.Data2 == guidOther.Data2 &&
        guidOne.Data3 == guidOther.Data3 &&
        std::equal(guidOne.Data4, guidOne.Data4 + sizeof(guidOne.Data4), guidOther.Data4);
}

inline bool IsOn(mfxU32 opt)
{
    return opt == MFX_CODINGOPTION_ON;
}

inline bool IsOff(mfxU32 opt)
{
    return opt == MFX_CODINGOPTION_OFF;
}

inline bool IsAdapt(mfxU32 opt)
{
    return opt == MFX_CODINGOPTION_ADAPTIVE;
}


inline void* AlignedMalloc(size_t size, size_t alignment)
{
    return aligned_alloc(alignment, size);
}

inline void AlignedFree(void* memory)
{
    free(memory);
}

inline bool check_import_flags(size_t flags)
{
    switch (flags)
    {
    case MFX_SURFACE_FLAG_DEFAULT:
    case MFX_SURFACE_FLAG_IMPORT_SHARED:
    case MFX_SURFACE_FLAG_IMPORT_COPY:
    case (MFX_SURFACE_FLAG_IMPORT_SHARED | MFX_SURFACE_FLAG_IMPORT_COPY):
        return true;
    default:
        return false;
    }
}

inline bool check_export_flags(size_t flags)
{
    switch (flags)
    {
    case MFX_SURFACE_FLAG_DEFAULT:
    case MFX_SURFACE_FLAG_EXPORT_SHARED:
    case MFX_SURFACE_FLAG_EXPORT_COPY:
    case (MFX_SURFACE_FLAG_EXPORT_SHARED | MFX_SURFACE_FLAG_EXPORT_COPY):
        return true;
    default:
        return false;
    }
}

#endif // __MFXUTILS_H__
