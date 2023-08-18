// Copyright (c) 2020-2023 Intel Corporation
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

#include <algorithm>
#include <iterator>
#include <cmath>
#include <string.h>
#include "aenc++.h"
#include <memory>

#if defined(MFX_ENABLE_ADAPTIVE_ENCODE)

mfxStatus AEncInit(mfxHDL* pthis, AEncParam param) {
    if (!pthis) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    try {
        std::unique_ptr<aenc::AEnc> a(new aenc::AEnc());

        a->Init(param);

        *pthis = reinterpret_cast<mfxHDL>(a.release());
        return MFX_ERR_NONE;
    }
    catch (aenc::Error) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    catch (...)
    {
        return MFX_ERR_UNKNOWN;
    }
}


void AEncClose(mfxHDL pthis) {
    if (!pthis) {
        return;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        a->Close();
        delete a;
        return;
    }
    catch (aenc::Error) {
        return;
    }
    catch (...)
    {
        return;
    }
}


mfxStatus AEncProcessFrame(mfxHDL pthis, mfxU32 POC, mfxU8* InFrame, mfxI32 pitch, AEncFrame* OutFrame) {
    if (!pthis) {
        return MFX_ERR_NOT_INITIALIZED;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->ProcessFrame(POC, InFrame, pitch, OutFrame);
    }
    catch (aenc::Error) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    catch (...)
    {
        return MFX_ERR_UNKNOWN;
    }
}

mfxU16 AEncGetIntraDecision(mfxHDL pthis, mfxU32 displayOrder) {
    if (!pthis) {
        return 0;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->GetIntraDecision(displayOrder);
    }
    catch (aenc::Error) {
        return 0;
    }
    catch (...)
    {
        return 0;
    }
}

mfxU16 AEncGetPersistenceMap(mfxHDL pthis, mfxU32 displayOrder, mfxU8 PMap[AENC_MAP_SIZE]) {
    if (!pthis) {
        return 0;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->GetPersistenceMap(displayOrder, PMap);
    }
    catch (aenc::Error) {
        return 0;
    }
    catch (...)
    {
        return 0;
    }
}

mfxU16 AEncGetLastPQp(mfxHDL pthis) {
    if (!pthis) {
        return 0;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->GetLastPQp();
    }
    catch (aenc::Error) {
        return 0;
    }
    catch (...)
    {
        return 0;
    }
}

mfxI8 AEncAPQSelect(mfxHDL pthis, mfxU32 SC, mfxU32 TSC, mfxU32 MVSize, mfxU32 Contrast, mfxU32 PyramidLayer, mfxU32 BaseQp) {
    if (!pthis) {
        return 0;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->APQPredict(SC, TSC, MVSize, Contrast, PyramidLayer, BaseQp);
    }
    catch (aenc::Error) {
        return 0;
    }
    catch (...)
    {
        return 0;
    }
}

void AEncUpdateFrame(mfxHDL pthis, mfxU32 displayOrder, mfxU32 bits, mfxU32 QpY, mfxU32 Type) {
    if (!pthis) {
        return;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->UpdateFrame(displayOrder, bits, QpY, Type);
    }
    catch (aenc::Error) {
        return;
    }
    catch (...)
    {
        return;
    }
}


namespace aenc {
    const mfxU16 APQ_Lookup_AVC[4][3][3][6] = {
        {{{3,3,3,3,3,2},{1,1,2,2,2,1},{3,2,2,2,2,1}},
         {{2,3,3,3,3,2},{1,1,2,2,2,2},{3,2,2,2,2,1}},
         {{2,2,3,3,2,2},{1,1,2,2,2,2},{2,1,2,2,2,2}}},
        {{{2,2,2,3,3,2},{2,2,2,1,1,1},{2,2,1,1,1,1}},
         {{2,2,2,2,1,1},{2,2,2,2,1,1},{3,2,2,1,1,1}},
         {{2,2,2,2,2,2},{3,2,2,1,1,1},{2,2,2,2,1,1}}},
        {{{2,2,2,2,2,2},{2,2,2,2,2,2},{2,2,2,2,1,1}},
         {{2,2,2,2,2,2},{2,2,2,2,2,2},{2,2,2,2,2,1}},
         {{2,2,2,2,2,2},{2,2,2,2,2,2},{1,1,2,2,1,1}}},
        {{{3,2,2,2,2,2},{3,2,2,2,2,2},{2,2,2,2,2,1}},
         {{2,3,3,3,2,2},{2,2,2,2,2,2},{2,2,2,2,2,1}},
         {{2,2,2,2,2,2},{2,2,2,2,2,2},{2,2,2,2,2,2}}}};

    const mfxU16 APQ_Lookup_HEVC[4][3][3][6] = {
        {{{3,3,3,3,3,2},{1,1,0,0,1,1},{3,2,2,2,2,2}},
         {{3,3,3,3,3,2},{3,0,0,0,1,2},{1,1,2,2,2,2}},
         {{0,0,3,3,2,2},{1,1,0,0,1,1},{0,1,1,1,1,1}}},
        {{{2,2,2,3,3,2},{2,2,1,2,2,2},{2,2,1,2,2,2}},
         {{3,2,2,2,2,1},{3,3,1,1,2,2},{1,1,1,1,2,2}},
         {{3,3,2,2,2,1},{3,3,1,1,2,2},{1,1,1,1,2,2}}},
        {{{2,2,2,2,2,2},{2,2,2,2,2,2},{0,0,0,2,2,2}},
         {{2,2,2,2,2,1},{2,2,2,2,2,2},{2,2,2,2,2,2}},
         {{2,1,1,2,2,2},{2,2,2,2,2,2},{1,2,2,2,2,2}}},
        {{{3,2,2,2,2,2},{3,2,2,2,2,2},{2,2,0,0,0,0}},
         {{2,3,3,3,2,2},{2,2,2,2,2,2},{2,2,2,2,2,2}},
         {{2,2,2,2,2,2},{2,2,2,2,2,2},{2,2,2,2,2,2}}}};

    int32_t moving_average(int32_t val, int32_t avg, int32_t N) {
        if (avg <= 0 || N == 0) return val;
        int newAvg = avg + (val - avg) / N;
        return newAvg;
    }
    int32_t quant_sc(int32_t val) {
        int32_t qval = (val < 3500) ? (val < 2000 ? 0 : 1) : (val < 7500 ? 2 : 3);
        return qval;
    }
    int32_t quant_contrast(int32_t val) {
        int32_t qval = (val < 35) ? 0 : ((val < 65) ? 1 : 2);
        return qval;
    }
    int32_t quant_tsc(int32_t val) {
        val >>= 10;
        int32_t qval;
        if (val < 300) {
            qval = (val < 60) ? 0 : ((val < 200) ? 1 : 2);
        }
        else {
            qval = (val < 500) ? 3 : ((val < 900) ? 4 : 5);
        }
        return qval;
    }
    int32_t quant_mv(int32_t val) {
        int32_t qval = (val < 400) ? 0 : (val < 1500 ? 1 : 2);
        return qval;
    }



    InternalFrame::operator ExternalFrame() {
        ExternalFrame ext;
        ext.POC = POC;
        ext.QpY = QpY;
        ext.SceneChanged = SceneChanged;
        ext.RepeatedFrame = RepeatedFrame;
        ext.TemporalComplexity = TemporalComplexity;
        ext.SpatialComplexity = SpatialComplexity;
        ext.LTR = LTR;
        ext.MiniGopSize = MiniGopSize;
        ext.PyramidLayer = PyramidLayer;
        ext.Type = Type;
        ext.DeltaQP = DeltaQP;
        ext.ClassAPQ = ClassAPQ;
        ext.QPDeltaExplicitModulation = QPDeltaExplicitModulation;
        ext.KeepInDPB = KeepInDPB;
        ext.RemoveFromDPB = RemoveFromDPB;
        ext.RefList = RefList;
        ext.LongTermRefList = LongTermRefList;
        memcpy(ext.PMap, PMap, sizeof(ext.PMap));
        memcpy(ext.FeaturesAPQ, FeaturesAPQ, sizeof(ext.FeaturesAPQ));
        return ext;
    }


    ExternalFrame::operator AEncFrame() {
        AEncFrame out;
        out.POC = POC;
        out.QpY = QpY;
        out.SceneChanged = SceneChanged;
        out.RepeatedFrame = RepeatedFrame;
        out.TemporalComplexity = TemporalComplexity;
        out.LTR = LTR!=LTRType::NONE;
        out.MiniGopSize = MiniGopSize;
        out.PyramidLayer = PyramidLayer;
        out.DeltaQP = DeltaQP;
        out.ClassAPQ = ClassAPQ;
        out.QPDeltaExplicitModulation = QPDeltaExplicitModulation;
        out.SpatialComplexity = SpatialComplexity;
        out.KeepInDPB = KeepInDPB;

        switch (Type) {
        case FrameType::IDR:
            out.Type = MFX_FRAMETYPE_IDR;
            break;
        case FrameType::I:
            out.Type = MFX_FRAMETYPE_I;
            break;
        case FrameType::P:
            out.Type = MFX_FRAMETYPE_P;
            break;
        case FrameType::B:
            out.Type = MFX_FRAMETYPE_B;
            break;
        default:
            throw Error("wrong frame type in ExternalFrame::operator AEncFrame()");

        }

        if (RemoveFromDPB.size() > sizeof(out.RemoveFromDPB) / sizeof(out.RemoveFromDPB[0])) {
            throw Error("wrong RemoveFromDPB size");
        }
        out.RemoveFromDPBSize = static_cast<mfxU32>(RemoveFromDPB.size());
        std::copy(RemoveFromDPB.begin(), RemoveFromDPB.end(), out.RemoveFromDPB);

        if (RefList.size() > sizeof(out.RefList) / sizeof(out.RefList[0])) {
            throw Error("wrong RefList size");
        }
        out.RefListSize = static_cast<mfxU32>(RefList.size());
        std::copy(RefList.begin(), RefList.end(), out.RefList);

        if (LongTermRefList.size() > sizeof(out.LongTermRefList) / sizeof(out.LongTermRefList[0])) {
            throw Error("wrong LongTermRefList size");
        }
        out.LongTermRefListSize = static_cast<mfxU32>(LongTermRefList.size());
        std::copy(LongTermRefList.begin(), LongTermRefList.end(), out.LongTermRefList);

        memcpy(out.PMap, PMap, sizeof(out.PMap));
        memcpy(out.FeaturesAPQ, FeaturesAPQ, sizeof(out.FeaturesAPQ));

        return out;
    }


    void ExternalFrame::print() {
        //if (SceneChanged) {
            printf(" frame[%4d] %s %s %s %s TC %d gop %d pyr %d qp %3d %s remove %4d:%d ref %4d:%d ltr ref %4d:%d\n",
                POC,
                Type == FrameType::IDR ? "IDR" :
                Type == FrameType::I ? "I  " :
                Type == FrameType::P ? " P " :
                Type == FrameType::B ? "  B" :
                "UND",
                SceneChanged ? "SCD" : "   ",
                RepeatedFrame ? "R" : " ",
                LTR!=LTRType::NONE ? "LTR" : "   ",
                TemporalComplexity,
                MiniGopSize,
                PyramidLayer, DeltaQP,
                KeepInDPB ? "keep":"    ",
                RemoveFromDPB.size() < 1 ? -1 : RemoveFromDPB[0],
                RemoveFromDPB.size() < 2 ? -1 : RemoveFromDPB[1],
                RefList.size() < 1 ? -1 : RefList[0],
                RefList.size() < 2 ? -1 : RefList[1],
                LongTermRefList.size() < 1 ? -1 : LongTermRefList[0],
                LongTermRefList.size() < 2 ? -1 : LongTermRefList[1]);
        //}
    }


    void AEnc::Init(AEncParam param) {
        //check param

        if (param.MaxMiniGopSize != 1 && param.MaxMiniGopSize != 2 && param.MaxMiniGopSize != 4 && param.MaxMiniGopSize != 8 && param.MaxMiniGopSize != 16) {
            throw Error("wrong MaxMiniGopSize at Init");
        }

        if (param.MinGopSize >= param.MaxGopSize || param.MaxGopSize > param.MaxIDRDist || param.MaxIDRDist % param.MaxGopSize != 0) {
            //if this condition is changed, then MakeIFrameDecision() should be updated accordingly
            throw Error("wrong GOP size at Init");
        }

        if (param.MinGopSize > param.MaxGopSize - param.MaxMiniGopSize) {
            throw Error("wrong adaptive GOP parameters at Init");
        }

        if (param.ColorFormat != MFX_FOURCC_NV12 && param.ColorFormat != MFX_FOURCC_RGB4) {
            throw Error("wrong color format at Init");
        }

        //save params
        InitParam = param;

        //init SCD
        mfxStatus sts = Scd.Init(param.FrameWidth, param.FrameHeight, param.Pitch, 0/*progressive*/, param.ColorFormat == MFX_FOURCC_NV12, param.CodecId);
        if (sts != MFX_ERR_NONE) {
            throw Error("SCD initialization failed");
        }
        sts = LtrScd.Init(param.FrameWidth, param.FrameHeight, param.Pitch, 0/*progressive*/, param.ColorFormat == MFX_FOURCC_NV12, param.CodecId);
        if (sts != MFX_ERR_NONE) {
            throw Error("LtrSCD initialization failed");
        }
        LastPFrameQp = 0;
        LastPFramePOC = 0;
        m_isLtrOn = InitParam.ALTR ? true : false;
        memset(m_PersistenceMap, 0, sizeof(m_PersistenceMap));

    }


    void AEnc::Close(){
        Scd.Close();
        LtrScd.Close();
    }


    mfxStatus AEnc::ProcessFrame(uint32_t POC, const uint8_t* InFrame, int32_t pitch, AEncFrame* OutFrame) {
        InternalFrame f;
        f.POC = POC;
        f.PPyramidIdx = 0;
        f.LTR = LTRType::NONE;

        if (InFrame != nullptr) {
            //run SCD, generates stat, saves GOP size and SC
            RunScd(InFrame, pitch, f);

            //save per frame stat
            SaveStat(f);

            MakeIFrameDecision(f);

            //save frame to buffer
            FrameBuffer.push_back(f);
        }
        else {
            //EOS
            f.Type = FrameType::DUMMY;
            while (FrameBuffer.size() < InitParam.MaxMiniGopSize) {
                FrameBuffer.push_back(f);
            }
        }


        //make mini GOP decision
        uint32_t MiniGopSize;
        if (MakeMiniGopDecision(MiniGopSize)) {
            //GOP decided, compute ref list and QP for each frame in mini-GOP
            for (uint32_t i = 0; i < MiniGopSize; i++) {
                f = FrameBuffer.front();
                FrameBuffer.pop_front();

                MarkFrameInMiniGOP(f, MiniGopSize, i);

                ComputeStat(f);

                MakeAltrArefDecision(f);

                BuildRefList(f);
                AdjustQp(f);
                MakeDbpDecision(f);
                SaveFrameTypeInfo(f);

                OutputBuffer.push_back(f);
            }
        }

        //output frame
        return OutputDecision(OutFrame);
    }


    void AEnc::RunScd(const uint8_t* InFrame, int32_t pitch, InternalFrame &f) {
        Scd.PutFrameProgressive(const_cast<uint8_t*>(InFrame), pitch, false/*LTR*/);

        f.MiniGopSize = Scd.GetFrameGopSize();
        mfxU32 codecID = InitParam.CodecId;
        if (codecID == MFX_CODEC_HEVC || codecID == MFX_CODEC_AVC || codecID == MFX_CODEC_AV1) {
            if (InitParam.MaxMiniGopSize == 2 || f.MiniGopSize < 4) {
                f.MiniGopSize = Scd.CorrectScdMiniGopDecision();
            }
        }

        f.SceneChanged = Scd.Get_frame_shot_Decision();
        f.RepeatedFrame = Scd.Get_Repeated_Frame_Flag();
        f.TemporalComplexity = Scd.Get_frame_Temporal_complexity();
        f.SpatialComplexity = Scd.Get_frame_SC();
        f.UseLtrAsReference = true;

        Scd.GetImageAndStat(f.ScdImage, f.ScdStat, ASCReference_Frame, ASCprevious_frame_data);
        Scd.get_PersistenceMap(f.PMap, false);
        Scd.get_PersistenceMap(m_PersistenceMap, true);
    }


    void AEnc::SaveStat(InternalFrame& f){
        SaveAltrStat(f);
        SaveArefStat(f);
        SaveApqStat(f);
    }


   void AEnc::SaveAltrStat(InternalFrame& f) {
        f.MV = (int32_t)Scd.Get_frame_MV0();
        f.highMvCount = Scd.Get_frame_RecentHighMvCount();
        ASC_LTR_DEC scd_LTR_hint = NO_LTR;
        Scd.get_LTR_op_hint(scd_LTR_hint);
        f.LtrOnHint = (scd_LTR_hint == NO_LTR) ? 0 : 1;
    }


    void AEnc::SaveArefStat(InternalFrame& f) {
        f.MV = (int32_t)Scd.Get_frame_MV0();
        f.CORR = Scd.Get_frame_mcTCorr();
    }


    void AEnc::SaveApqStat(InternalFrame& f) {
        f.SC = Scd.Get_frame_Spatial_complexity();
        f.TSC = Scd.Get_frame_Temporal_complexity();
        f.MVSize = Scd.Get_frame_MVSize();
        f.Contrast = Scd.Get_frame_Contrast();
        f.SC0 = Scd.Get_frame_SC();
        f.TSC0 = Scd.Get_frame_SpatioTemporal_complexity();
    }


    void AEnc::MakeIFrameDecision(InternalFrame& f) {
        //function should be called strictly before MakeMiniGopDecision()
        //first frame in sequence
        if (f.POC == 0) {
            MarkFrameAsIDR(f);
            return;
        }

        //strict I decision
        if (InitParam.StrictIFrame) {
            if (f.POC % InitParam.GopPicSize == 0) {
                if (f.POC % InitParam.MaxIDRDist == 0) {
                    MarkFrameAsIDR(f);
                }
                else {
                    MarkFrameAsI(f);
                }
            }
            return;
        }

        //protected interval between I frames.
        const uint32_t CurrentGOPSize = f.POC - PocOfLastIFrame;
        if (CurrentGOPSize < InitParam.MinGopSize) {
            //GOP is too small, keep current frame as P, even if it is scene change
            return;
        }

        //insert IDR if max IDR interval reached
        uint32_t CurrentIdrInterval = f.POC - PocOfLastIdrFrame;
        if (CurrentIdrInterval >= InitParam.MaxIDRDist) {
            MarkFrameAsIDR(f);
            return;
        }

        // For AVC use IDR, for HEVC use CRA
        if (f.SceneChanged && (InitParam.CodecId == MFX_CODEC_AVC || InitParam.CodecId == MFX_CODEC_AV1)) {
            MarkFrameAsIDR(f);
            return;
        }

        //scene changed or max GOP size reached, insert I
        if (f.SceneChanged || CurrentGOPSize >= InitParam.MaxGopSize) {
            MarkFrameAsI(f);
        }
    }


    bool AEnc::MakeMiniGopDecision(uint32_t& GOPDecision) {
        if (FrameBuffer.size() < InitParam.MaxMiniGopSize) {
            return false;
        }

        //EOS
        if (FrameBuffer[0].Type == FrameType::DUMMY) {
            return false;
        }

        GOPDecision = std::min(GetMiniGopSizeCommon(), GetMiniGopSizeAGOP());
        return true;
    }


    uint32_t AEnc::GetMiniGopSizeCommon() {
        //find start of next mini GOP
        uint32_t n = 1;
        for (InternalFrame f : FrameBuffer) {
            //check if we need additional P frame
            //first frame can't be dummy, scene change frame is usually I, except cases when it is too close to previous I
            if (f.Type == FrameType::IDR || f.Type == FrameType::DUMMY || (!InitParam.StrictIFrame && f.SceneChanged)) {
                n = (n == 1 ? 1 : n - 1);
                break;
            }

            //no additional P, just close mini GOP
            if (f.Type == FrameType::I) {
                break;
            }
            n++;
        }
        //n is mini GOP size from B to P/I inclusive
        return n;
    }


    uint32_t AEnc::GetMiniGopSizeAGOP() {
        //function tries to assemble as long mini GOP as possible, ignoring
        //occasional frames with smaller by one GOP size
        if (!InitParam.AGOP) {
            return InitParam.MaxMiniGopSize;
        }

        for (uint32_t CurMiniGOPSize = InitParam.MaxMiniGopSize; CurMiniGOPSize > 1; CurMiniGOPSize /= 2) {
            uint32_t FullSizeCount = 0, HalfSizeCount = 0, FrameCount = 0;
            for (; FrameCount < CurMiniGOPSize; FrameCount++) {
                if (FrameBuffer[FrameCount].MiniGopSize >= CurMiniGOPSize) {
                    FullSizeCount++;
                }
                if (FrameBuffer[FrameCount].MiniGopSize == CurMiniGOPSize / 2) {
                    HalfSizeCount++;
                }
                if (FrameBuffer[FrameCount].MiniGopSize <= CurMiniGOPSize / 4) {
                    break;
                }
            }

            if (FrameCount <= CurMiniGOPSize / 2) {
                //not enough frames for current mini GOP size, try smaller one
                continue;
            }
            if (CurMiniGOPSize<=8 && FullSizeCount <= HalfSizeCount) {
                //not enough frames with long mini GOP size, try smaller GOP
                continue;
            }
            return FrameCount;
        }

        return 1; //we tried everything else

    }

    void AEnc::SaveFrameTypeInfo(InternalFrame& f) {
        if (FrameBuffer.size())
        {
            FrameBuffer.front().PrevType = f.Type;
            FrameBuffer.front().PPyramidLayer = f.PPyramidLayer;
            FrameBuffer.front().PPyramidIdx = f.PPyramidIdx;
            if (f.PrevType == FrameType::P && f.Type == FrameType::B && OutputBuffer.size())
                OutputBuffer.back().DeltaQP = 0;
        }
    }

    void AEnc::ComputeStat(InternalFrame& f) {
        if (InitParam.ALTR) ComputeStatAltr(f);
        if (InitParam.AREF) ComputeStatAref(f);
        if (InitParam.APQ) ComputeStatApq(f);
    }


    void AEnc::MakeDbpDecision(InternalFrame& f) {
        MakeDbpDecisionRef(f);
        if (InitParam.ALTR) MakeDbpDecisionLtr(f);
        if (InitParam.AREF) MakeDbpDecisionAref(f);
    }

    void AEnc::BuildRefList(InternalFrame& f) {
        if (InitParam.ALTR) BuildRefListLtr(f);
        if (InitParam.AREF) BuildRefListAref(f);
    }


    void AEnc::AdjustQp(InternalFrame& f) {
        f.DeltaQP = 0;
        if (InitParam.ALTR) AdjustQpLtr(f);
        if (InitParam.AREF) AdjustQpAref(f);
        if (InitParam.APQ) AdjustQpApq(f);
        if (InitParam.AGOP) {
            if (!InitParam.ALTR && !InitParam.AREF && !InitParam.APQ)
                AdjustQpAgop(f);
        }
    }

    void AEnc::MakeAltrArefDecision(InternalFrame& f) {
        if (InitParam.ALTR) MakeAltrDecision(f);
        if (InitParam.AREF) MakeArefDecision(f);
    }


    void AEnc::ComputeStatAltr(InternalFrame& f) {
        if (f.Type == FrameType::I || f.Type == FrameType::IDR || f.LTR!=LTRType::NONE) {
            return;
        }


        mfxI32 iMV = f.MV;
        if (iMV > 4000) iMV = 4000;
        mfxI32 prevAvgMV0 = m_avgMV0;
        if (m_avgMV0 > 8) {
            m_avgMV0 = prevAvgMV0 + (iMV - prevAvgMV0) / 4;
        }


        mfxU32 sceneTransition = 0;
        //check if current frame should use LTR as reference
        if(f.LTR == LTRType::NONE){
            LtrScd.SetImageAndStat(f.ScdImage, f.ScdStat, ASCCurrent_Frame, ASCcurrent_frame_data);
            LtrScd.RunFrame_LTR();

            mfxU32 sctrFlag = LtrScd.Get_scene_transition_Decision();
            if (f.POC <= 16)
                sctrFlag = 0;
            m_sceneTranBuffer[f.POC % 8] = sctrFlag;
            mfxI32 cnt = 0;
            for (mfxI32 i = 0; i < 8; i++) {
                if (m_sceneTranBuffer[i])
                    cnt++;
            }
            sceneTransition = (cnt == 8) ? 1 : 0;
        }

        f.SceneTransition = sceneTransition ? true : false;
        if (f.SceneTransition) {
            m_isLtrOn = false;
        }
    }



    void AEnc::ComputeStatAref(InternalFrame& f) {
        if (f.Type != FrameType::I && f.Type != FrameType::IDR)
        {
            m_hasLowActivity = 0;
            m_prevTemporalActs[f.MiniGopIdx] = (f.MV > 1000) ? 1 : 0;
            mfxI32 cnt = 0;
            for (mfxI32 i = 0; i < 8; i++) {
                if (m_prevTemporalActs[i])
                    cnt++;
            }
            m_hasLowActivity = (cnt < 3) ? 1 : 0;
        }
        else
        {
            m_hasLowActivity = 0;
            for (mfxI32 i = 0; i < 8; i++)
                m_prevTemporalActs[i] = 0;
        }
    }

    const mfxF64 DefModel[10][10][3] = {
        // 0                    1                     2                     3                    4          5          6          7          8          9
        {{ 0,     0,     0 }, { 0,      0,     0 }, { 0,      0,     0 }, { 0,     0,     0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //0
        {{ 0,     0,     0 }, { 0,      0,     0 }, { 0,      0,     0 }, { 0,     0,     0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //1
        {{ 0,     0,     0 }, { 0.2167, 0.4914,1 }, { 0,      0,     0 }, { 0,     0,     0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //2
        {{ 0.0830,0.6201,1 }, { 0.0916, 0.7462,1 }, { 0.3533, 0.5491,1 }, { 0,     0,     0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //3
        {{ 0.1455,0.4302,1 }, { 0.0580, 0.7937,1 }, { 0.4327, 0.4359,1 }, { 0.2197,0.7141,1 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //4
        {{ 0,     0,     0 }, { 0.1136, 0.7446,1 }, { 0.1770, 0.6730,1 }, { 0.0139,1.4547,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //5
        {{ 0.0617,0.8463,0 }, { 0.0454, 0.9545,0 }, { 0.4038, 0.4899,1 }, { 0.2234,0.7087,1 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //6
        {{ 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //7
        {{ 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //8
        {{ 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}  //9
    };

    void AEnc::UpdateFrame(uint32_t frm, uint32_t /*size*/, uint32_t qp, uint32_t Type)
    {
        InternalFrame *fi =  FindInternalFrame(frm);
        if (fi)
        {
            //printf("Found Internal Frame %d set qp %d", frm, qp);
            fi->QpY = (uint8_t)qp;
            if (fi->Type != FrameType::P) return;
        }
        else 
        {
            ExternalFrame* fe = FindExternalFrame(frm);
            if (fe)
            {
                //printf("Found External Frame  %d set qp %d", frm, qp);
                fe->QpY = (uint8_t)qp;
                if (fe->Type != FrameType::P) return;
            }
            else {
                auto fd = std::find_if(DpbBuffer.begin(), DpbBuffer.end(), [frm](InternalFrame& x) {return x.POC == frm; });
                if (fd != DpbBuffer.end())
                {
                    //printf("Found DPB Buffer Frame %d set qp %d\n", frm, qp);
                    fd->QpY = (uint8_t)qp;
                    if (fd->Type != FrameType::P) return;
                }
            }
        }
        if(Type == (uint32_t) MFX_FRAMETYPE_P)
        {
            LastPFrameQp = qp;
            LastPFramePOC = frm;
        }
    }

    InternalFrame* AEnc::FindInternalFrame(mfxU32 displayOrder)
    {
        auto outframe = std::find_if(FrameBuffer.begin(), FrameBuffer.end(), [&](auto& x) {return x.POC == displayOrder; });
        return (outframe == FrameBuffer.end() ? nullptr : &*outframe);
    }

    ExternalFrame* AEnc::FindExternalFrame(mfxU32 displayOrder)
    {
        auto outframe = std::find_if(OutputBuffer.begin(), OutputBuffer.end(), [&](auto& x) {return x.POC == displayOrder; });
        return (outframe == OutputBuffer.end() ? nullptr : &*outframe);
    }

    mfxU16 AEnc::GetIntraDecision(mfxU32 displayOrder)
    {
        FrameType ftype = FrameType::UNDEF;
        auto internalMatchFrame = FindInternalFrame(displayOrder);
        if (internalMatchFrame) 
        {
            ftype = internalMatchFrame->Type;
        }
        else 
        {
            auto externalMatchFrame = FindExternalFrame(displayOrder);
            if (externalMatchFrame)
            {
                ftype = externalMatchFrame->Type;
            }
        }
        switch (ftype) {
        case FrameType::IDR:
            return MFX_FRAMETYPE_IDR;
            break;
        case FrameType::I:
            return MFX_FRAMETYPE_I;
            break;
        default:
            return 0;
        }
    }

    mfxU16 AEnc::GetPersistenceMap(mfxU32 , mfxU8 PMap[ASC_MAP_SIZE])
    {
        mfxU16 count = 0;
        for (mfxU32 i = 0; i < ASC_MAP_SIZE; i++) {
            PMap[i] = m_PersistenceMap[i]; // (has async issues)
            if (PMap[i]) count++;
        }
        return count;
    }

    mfxU16 AEnc::GetLastPQp()
    {
        return (mfxU16)LastPFrameQp;
    }

    mfxI8 AEnc::APQPredict(mfxU32 SC, mfxU32 TSC, mfxU32 MVSize, mfxU32 Contrast, mfxU32 PyramidLayer, mfxU32 BaseQp)
    {
        return APQSelect(SC, TSC, MVSize, Contrast, PyramidLayer, BaseQp);
    }

    void AEnc::ComputeStatApq(InternalFrame& f) {
        avgSC = moving_average(f.SC0, avgSC, 8);
        avgTSC = moving_average(f.TSC0, avgTSC, 8);
        avgMV = moving_average(f.MVSize, avgMV, 8);
        int32_t qsc = quant_sc(avgSC);
        int32_t qtsc = quant_tsc(avgTSC);
        int32_t qmv = quant_mv(avgMV);
        int32_t qcon = quant_contrast(f.Contrast);
        if (InitParam.CodecId == MFX_CODEC_HEVC || InitParam.CodecId == MFX_CODEC_AV1) {
            f.ClassAPQ = (int32_t)APQ_Lookup_HEVC[qsc][qcon][qmv][qtsc];
        }
        else {
            f.ClassAPQ = (int32_t)APQ_Lookup_AVC[qsc][qcon][qmv][qtsc];
        }
        f.FeaturesAPQ[0] = f.SC0;
        f.FeaturesAPQ[1] = f.TSC0;
        f.FeaturesAPQ[2] = f.MVSize;
        f.FeaturesAPQ[3] = f.Contrast;
        //printf("FRM %d SC %d TSC %d MVSize %d MVQ %d Contrast %d SCTSC %d APQ %d\n", f.POC, SC, TSC, MVSize, MVQ, Contrast, f.ClassSCTSC, f.ClassAPQ);
    }

    void AEnc::MakeDbpDecisionRef(InternalFrame& f) {
        if (f.Type != FrameType::B && f.LTR == LTRType::NONE) {
            // Rotate
            if (DpbBuffer.size() < InitParam.NumRefP) {
                DpbBuffer.push_back(f);
            }
            else if(DpbBuffer.size() > 1) {
                auto fp = std::min_element(DpbBuffer.begin(), DpbBuffer.end(), [=](InternalFrame& a, InternalFrame& smallest) { return (smallest.LTR!=LTRType::NONE ? true : (a.LTR!=LTRType::NONE ? false : (a.POC < smallest.POC))); });
                if (fp != DpbBuffer.end()) {
                    DpbBuffer.erase(fp);
                    DpbBuffer.push_back(f);
                }
            }
        }
    }

    void AEnc::MakeDbpDecisionLtr(InternalFrame& f) {
        //if current frame is LTR remove old LTR from DPB
        if (f.LTR == LTRType::ALTR) {
            RemoveFrameFromDPB(f, [=](InternalFrame& x) {return x.LTR!=LTRType::NONE; });

            //add new LTR to DPB
            f.KeepInDPB = true;
            DpbBuffer.push_back(f);
        }
    }


    void AEnc::MakeDbpDecisionAref(InternalFrame& f) {
        if (f.LTR == LTRType::AREF) {
            RemoveFrameFromDPB(f, [=](InternalFrame& x) {return x.LTR!=LTRType::NONE; });
            f.KeepInDPB = true;
            DpbBuffer.push_back(f);
        }
    }


    void AEnc::BuildRefListLtr(InternalFrame& f) {
        if (f.Type == FrameType::P && f.UseLtrAsReference)
        {
            auto LtrFrame = std::find_if(DpbBuffer.begin(), DpbBuffer.end(), [](InternalFrame& x) {return x.LTR!=LTRType::NONE; });
            if (LtrFrame != DpbBuffer.end() && LtrFrame->LTR == LTRType::ALTR) {
                f.RefList.push_back(LtrFrame->POC);
            }
        }
    }

    void AEnc::BuildRefListAref(InternalFrame& f) {
        if (f.Type == FrameType::P)
        {
            auto KeyPFrame = std::find_if(DpbBuffer.begin(), DpbBuffer.end(), [](InternalFrame& x) {return x.LTR!=LTRType::NONE; });
            if (KeyPFrame != DpbBuffer.end() && KeyPFrame->LTR == LTRType::AREF) {
                f.RefList.push_back(KeyPFrame->POC);
            }
        }
    }


    void AEnc::AdjustQpLtr(InternalFrame& f) {
        if (f.Type != FrameType::B) {
            if (f.LTR == LTRType::ALTR) {
                //if current frame is LTR boost QP
                if (f.POC == 0) {
                    DeltaQpForLtrFrame = -4;
                }
                else {
                    DeltaQpForLtrFrame = (m_avgMV0 > 1500 || (f.POC - LastLtrPoc) < 32) ? (-2) : (-4);
                }

                f.DeltaQP = DeltaQpForLtrFrame;
            }
        }
        else if (f.Type == FrameType::B) {
            if (!InitParam.APQ) {
                //use default QP offset for non-LTR frame
                if ((f.MiniGopType == 4 || f.MiniGopType == 8 || f.MiniGopType == 16) && f.PyramidLayer) {
                    f.DeltaQP = f.PyramidLayer;
                }
            }
        }
    }


    void AEnc::AdjustQpAref(InternalFrame& f) {
        if (f.Type != FrameType::B) {
            if (f.LTR == LTRType::AREF) {
                f.DeltaQP = (f.SC > 4 && m_hasLowActivity) ? (-4) : (-2);
            }
        }
        else if (f.Type == FrameType::B) {  // only CQP will use this DeltaQP
            if (!InitParam.APQ) {
                //use default QP offset for non-LTR frame
                if ((f.MiniGopType == 4 || f.MiniGopType == 8 || f.MiniGopType == 16) && f.PyramidLayer) {
                    f.DeltaQP = f.PyramidLayer;
                }
            }
        }
    }


    void AEnc::AdjustQpApq(InternalFrame& f) {
        if (f.Type == FrameType::I || f.Type == FrameType::IDR || f.Type == FrameType::P) {
            return;
        }
        uint32_t GOPSize = f.MiniGopType;
        if (GOPSize >= 8)
        {
            uint32_t level = std::max(1u, std::min(4u, f.PyramidLayer));
            uint32_t clsAPQ = std::max(0, std::min(3, f.ClassAPQ));
            f.DeltaQP = 1;

            if (clsAPQ == 1) {
                switch (level) {
                case 4:
                    f.DeltaQP += 1;
                case 3:
                    f.DeltaQP += 2;
                case 2:
                    f.DeltaQP += 1;
                case 1:
                default:
                    f.DeltaQP += 2;
                    break;
                }
            }
            else if (clsAPQ == 2) {
                switch (level) {
                case 4:
                    f.DeltaQP += 1;
                case 3:
                    f.DeltaQP += 2;
                case 2:
                    f.DeltaQP += 1;
                case 1:
                default:
                    f.DeltaQP += 1;
                    break;
                }
            }
            else if (clsAPQ == 3) {
                switch (level) {
                case 4:
                    f.DeltaQP += 1;
                case 3:
                    f.DeltaQP += 1;
                case 2:
                    f.DeltaQP += 1;
                case 1:
                default:
                    f.DeltaQP -= 1;
                    break;
                }
            }
            else  {
                switch (level) {
                case 4:
                    f.DeltaQP += 1;
                case 3:
                    f.DeltaQP += 2;
                case 2:
                    f.DeltaQP += 1;
                case 1:
                default:
                    f.DeltaQP += 0;
                    break;
                }
            }
        }
        else if (GOPSize == 4)
        {
            f.DeltaQP = 1 + f.PyramidLayer; // Default
        }
        else // if (GOPSize == 2)
        {
            f.DeltaQP = 3;              // Default
        }
    }

    void AEnc::AdjustQpAgop(InternalFrame& f)
    {
        uint32_t GOPSize = f.MiniGopType;
        if (f.Type == FrameType::I || f.Type == FrameType::IDR || (f.Type == FrameType::P && GOPSize > 4)) {
            return;
        }
        if (f.PyramidLayer)
        {
            if (GOPSize >= 8)
            {
                f.DeltaQP = f.PyramidLayer + 1;
            }
            else if (GOPSize == 4)
            {
                f.DeltaQP = f.PyramidLayer + 1;
            }
            else if (GOPSize == 2)
            {
                f.DeltaQP = 4;
            }
        }
        else if (GOPSize > 1)
        {
            f.DeltaQP = 1;
        }
        else // if (GOPSize == 1)
        {
            f.DeltaQP = f.PPyramidLayer;
        }
    }

    mfxStatus AEnc::OutputDecision(AEncFrame* OutFrame) {
        if (OutputBuffer.empty()) {
            return MFX_ERR_MORE_DATA;
        }

        ExternalFrame out = OutputBuffer.front();
        OutputBuffer.pop_front();

        //OutFrame.print();

        //delay frame removal from DPB to I/P frame
        if (out.Type == FrameType::B) {
            //do not remove on B frames
            copy(out.RemoveFromDPB.begin(), out.RemoveFromDPB.end(), std::back_inserter(RemoveFromDPBDelayed));
            out.RemoveFromDPB.clear();
        }
        else {
            //remove frame
            copy(RemoveFromDPBDelayed.begin(), RemoveFromDPBDelayed.end(), std::back_inserter(out.RemoveFromDPB));
            RemoveFromDPBDelayed.clear();
        }

        //out.print();

        *OutFrame = out;
        return MFX_ERR_NONE;
    }


    void AEnc::MarkFrameAsI(InternalFrame& f) {
        f.Type = FrameType::I;
        PocOfLastIFrame = f.POC;
    }


    void AEnc::MarkFrameAsIDR(InternalFrame& f) {
        f.Type = FrameType::IDR;
        PocOfLastIdrFrame = PocOfLastIFrame = f.POC;
    }


    void AEnc::MarkFrameAsLTR(InternalFrame& f) {
        f.LTR = LTRType::ALTR;
        f.UseLtrAsReference = true;

        m_avgMV0 = 0;
        LastLtrPoc = LtrPoc;
        LtrPoc = f.POC;
        m_isLtrOn = true;

        LtrScd.SetImageAndStat(f.ScdImage, f.ScdStat, ASCReference_Frame, ASCprevious_frame_data);

        f.SceneTransition = 0;
        for (mfxI32 i = 0; i < 8; i++) {
            m_sceneTranBuffer[i] = 0;
        }

        return;
    }

    void AEnc::MakeAltrDecision(InternalFrame& f) {
        if (!InitParam.ALTR){
            return;
        }

        if(f.POC == 0){
            MarkFrameAsLTR(f);
            return;
        }

        if (f.Type == FrameType::IDR && (m_isLtrOn || f.LtrOnHint)) {
            MarkFrameAsLTR(f);
            return;
        }

        if (f.Type != FrameType::B && f.SceneChanged && f.POC > LtrPoc + 16) {
            //new LTR at SC
            MarkFrameAsLTR(f);
            return;
        }

        if(f.Type != FrameType::B && LastPFrameQp)
        {
            auto ltrframe = std::find_if(DpbBuffer.begin(), DpbBuffer.end(), [](InternalFrame& x) {return x.LTR!=LTRType::NONE; });
            if (ltrframe != DpbBuffer.end()) 
            {
                if (LastPFramePOC > ltrframe->POC && LastPFramePOC > PocOfLastIdrFrame && LastPFrameQp < ltrframe->QpY && f.LtrOnHint) 
                {
                    uint32_t refLTRPOC = LastPFramePOC;
                    auto refframe = std::find_if(DpbBuffer.begin(), DpbBuffer.end(), [refLTRPOC](InternalFrame& x) {return x.POC == refLTRPOC; });
                    if (refframe != DpbBuffer.end()) 
                    {
                        // new LTR at low QP reference
                        MarkFrameAsLTR(*refframe);
                        f.LongTermRefList.push_back((*refframe).POC);
                        RemoveFrameFromDPB(f, [=](InternalFrame& x) {return x.LTR!=LTRType::NONE; }); // Remove First
                        return;
                    }
                }
            }
        }
        // Temporary, LTR frame will not be referenced. LTR still stays in DPB
        if ((f.MV > 2300 || f.TSC > 1024 || (f.MV > 1024 && f.highMvCount > 6)) && f.SC>4)
        {
            //don't use LTR as reference for current frame
            f.UseLtrAsReference = false;
        }
        else {
            f.UseLtrAsReference = true;
        }

    }

    void AEnc::MakeArefDecision(InternalFrame &f) {
        if (f.LTR!=LTRType::NONE) {
            return;
        }

        if ((!InitParam.ALTR) && ((f.SceneChanged && f.Type != FrameType::B) || f.Type == FrameType::IDR)) {
            f.LTR = LTRType::AREF;
            PocOfLastArefKeyFrame = f.POC;
            return;
        }

        if (f.Type == FrameType::P)
        {
            if (!m_isLtrOn || (!InitParam.ALTR))
            {
                // set P frame to Key reference frame
                const uint32_t ArefKeyFrameInterval = 32;
                uint32_t minPocArefKeyFrame = std::max(LtrPoc, PocOfLastArefKeyFrame) + ArefKeyFrameInterval;
                
                if (f.POC >= minPocArefKeyFrame)
                {
                    f.LTR = LTRType::AREF;
                    PocOfLastArefKeyFrame = f.POC;
                }
            }
        }
    }

    void AEnc::MarkFrameInMiniGOP(InternalFrame& f, uint32_t MiniGopSize, uint32_t MiniGopIdx ) {
        uint32_t GopTableIdx[17] = { 0/*n/a*/, 0/*1*/, 1/*2*/, 2,2/*3-4*/, 3,3,3,3/*5-8*/, 4,4,4,4,4,4,4,4/*8-16*/};
        uint32_t PyramidLayer[5/*mini GOP size 1,2,4,8*/][16] = {
            {0},
            {1,0},
            {2,1,2,0},
            {3,2,3,1,3,2,3,0},
            {4,3,4,2,4,3,4,1,4,3,4,2,4,3,4,0}
        };
        uint32_t MiniGopType[5] = { 1, 2, 4, 8, 16 };

        if (MiniGopSize == 0 || MiniGopSize >= sizeof(GopTableIdx)/sizeof(GopTableIdx[0])) {
            throw Error("wrong MiniGopSize");
        }

        if (MiniGopIdx >= MiniGopSize) {
            throw Error("wrong MiniGopIdx");
        }

        // 0 - P, 3 - non-reference B
        f.MiniGopSize = MiniGopSize;
        f.MiniGopIdx = MiniGopIdx;
        const uint32_t TblIdx = GopTableIdx[MiniGopSize];
        f.MiniGopType = MiniGopType[TblIdx];
        f.PyramidLayer = (MiniGopIdx == MiniGopSize - 1 ? 0 : PyramidLayer[TblIdx][MiniGopIdx]);
        if (f.Type == FrameType::UNDEF) { //may be I or IDR or UNDEF
            f.Type = (f.PyramidLayer == 0 ? FrameType::P : FrameType::B);
        }

        uint32_t PPyramid[8] = {5,4,3,2,4,3,2,1};
        if (f.Type == FrameType::I || f.Type == FrameType::IDR)
        {
            f.PPyramidLayer = 0;
            f.PPyramidIdx = 0;
        }
        else if (f.PrevType != FrameType::B && f.Type == FrameType::P)
        {
            f.PPyramidIdx = f.PPyramidIdx > 6 ? 0 : f.PPyramidIdx + 1;
            f.PPyramidLayer = PPyramid[f.PPyramidIdx];
        }
    }


    template <typename Condition>
    void AEnc::RemoveFrameFromDPB(InternalFrame &f, Condition C) {
        auto fp = std::find_if(DpbBuffer.begin(), DpbBuffer.end(), C);
        if (fp != DpbBuffer.end()) {
            f.RemoveFromDPB.push_back(fp->POC);
            DpbBuffer.erase(fp);
        }
    }


    template <typename Condition>
    void AEnc::AddFrameToRefList(InternalFrame& f, Condition C) {
        auto fp = std::min_element(DpbBuffer.begin(), DpbBuffer.end(), C);
        if (fp != DpbBuffer.end()) {
            f.RefList.push_back(fp->POC);
        }
    }

} //namespace aenc

#endif // MFX_ENABLE_ADAPTIVE_ENCODE
