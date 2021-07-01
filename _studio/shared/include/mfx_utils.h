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
#include "mfx_timing.h"

#include <va/va.h>


#include <cassert>
#include <cstddef>
#include <algorithm>
#include <chrono>
#include <functional>

#if defined (MFX_ENV_CFG_ENABLE) || defined(MFX_TRACE_ENABLE)
#include <sstream>
#endif

#if defined (MFX_ENV_CFG_ENABLE) || defined(MFX_TRACE_ENABLE)
#include <sstream>
#endif

#ifndef MFX_DEBUG_TRACE
#define MFX_STS_TRACE(sts) sts
#else
template <typename T>
static inline T mfx_print_err(T sts, const char *file, int line, const char *func)
{
    if (sts)
    {
        printf("%s: %d: %s: Error = %d\n", file, line, func, sts);
    }
    return sts;
}
#define MFX_STS_TRACE(sts) mfx_print_err(sts, __FILE__, __LINE__, __FUNCTION__)
#endif

#define MFX_SUCCEEDED(sts)      (MFX_STS_TRACE(sts) == MFX_ERR_NONE)
#define MFX_FAILED(sts)         (MFX_STS_TRACE(sts) != MFX_ERR_NONE)
#define MFX_RETURN(sts)         { return MFX_STS_TRACE(sts); }
#define MFX_CHECK(EXPR, ERR)    { if (!(EXPR)) MFX_RETURN(ERR); }

#define MFX_CHECK_NO_RET(EXPR, STS, ERR){ if (!(EXPR)) { std::ignore = MFX_STS_TRACE(ERR); STS = ERR; } }

#define MFX_CHECK_STS(sts)              MFX_CHECK(MFX_SUCCEEDED(sts), sts)
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
    case UMC::UMC_ERR_NOT_ENOUGH_BUFFER: return MFX_ERR_NOT_ENOUGH_BUFFER;
    case UMC::UMC_ERR_NOT_ENOUGH_DATA:   return MFX_ERR_MORE_DATA;
    case UMC::UMC_ERR_SYNC:              return MFX_ERR_MORE_DATA; // need to skip bad frames
    default:                             return MFX_ERR_UNKNOWN;   // need general error code here
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
#if (MFX_VERSION >= 1027)
    if (surf->Info.FourCC == MFX_FOURCC_Y410)
    {
        return !surf->Data.Y410;
    }
    else
#endif
    {
        return !surf->Data.Y;
    }
}

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(PTR)   { if (PTR) { PTR->Release(); PTR = NULL; } }
#endif


#ifdef MFX_ENABLE_CPLIB
    #define IS_PROTECTION_CENC(val) (MFX_PROTECTION_CENC_WV_CLASSIC == (val) || MFX_PROTECTION_CENC_WV_GOOGLE_DASH == (val))
#else
    #define IS_PROTECTION_CENC(val) (false)
#endif

    #define IS_PROTECTION_ANY(val) IS_PROTECTION_CENC(val)

#define MFX_COPY_FIELD(Field)       buf_dst.Field = buf_src.Field
#define MFX_COPY_ARRAY_FIELD(Array) std::copy(std::begin(buf_src.Array), std::end(buf_src.Array), std::begin(buf_dst.Array))

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

// TODO: switch to std::clamp when C++17 support will be enabled
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

template<class T, T... args>
struct integer_sequence
{
    using value_type = T;
    static size_t size() { return (sizeof...(args)); }
};

template<size_t... args>
using index_sequence = mfx::integer_sequence<size_t, args...>;

template<size_t N, size_t ...S>
struct make_index_sequence_impl
    : make_index_sequence_impl<N - 1, N - 1, S...>
{};

template<size_t ...S>
struct make_index_sequence_impl<0, S...>
{
    using type = index_sequence<S...>;
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

template<size_t S>
using make_index_sequence = typename make_index_sequence_impl<S>::type;

template<typename TFunc, typename TTuple, size_t ...S >
inline typename mfx::result_of<TFunc>::type
    apply_impl(TFunc&& fn, TTuple&& t, mfx::index_sequence<S...>)
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
        , typename mfx::make_index_sequence<std::tuple_size<typename std::remove_reference<TTuple>::type>::value>());
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

private:
    std::chrono::steady_clock::time_point m_end;
};

template <typename Representation>
using TimerMs = Timer<std::chrono::milliseconds, Representation>;

class mfxStatus_exception : public std::exception
{
public:
    mfxStatus_exception(mfxStatus sts = MFX_ERR_NONE) : sts(sts) {}

    operator mfxStatus() const { return sts; }

    mfxStatus sts = MFX_ERR_NONE;
};

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

#endif // __MFXUTILS_H__
