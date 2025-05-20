/* ****************************************************************************** *\

Copyright (C) 2012-2024 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_trace_dump_structures.cpp

\* ****************************************************************************** */

#include "mfx_trace_dump.h"

std::string DumpContext::dump(const std::string structName, const mfxEncodeCtrl& EncodeCtrl)
{
    std::string str = "mfxEncodeCtrl " + structName + " : addr[" + ToHexFormatString(&EncodeCtrl) + "]" + " size[" + ToString(sizeof(EncodeCtrl)) + "]" + "\n";
    str += dump(structName + ".Header", EncodeCtrl.Header) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(EncodeCtrl.reserved) + "\n";
    str += structName + ".reserved1=" + ToString(EncodeCtrl.reserved1) + "\n";
    str += structName + ".MfxNalUnitType=" + ToString(EncodeCtrl.MfxNalUnitType) + "\n";
    str += structName + ".SkipFrame=" + ToString(EncodeCtrl.SkipFrame) + "\n";
    str += structName + ".QP=" + ToString(EncodeCtrl.QP) + "\n";
    str += structName + ".FrameType=" + ToString(EncodeCtrl.FrameType) + "\n";
    str += structName + ".NumPayload=" + ToString(EncodeCtrl.NumPayload) + "\n";
    str += structName + ".reserved2=" + ToString(EncodeCtrl.reserved2) + "\n";
    str += dump_mfxExtParams(structName, EncodeCtrl);
    str += structName + ".Payload=" + ToString(EncodeCtrl.Payload);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxVideoParam& videoParam)
{
    std::string str = "mfxVideoParam " + structName + " : addr[" + ToHexFormatString(&videoParam) + "]" + " size[" + ToString(sizeof(videoParam)) + "]" + "\n";
    str += structName + ".AllocId=" + ToString(videoParam.AllocId) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(videoParam.reserved) + "\n";
    str += structName + ".reserved3=" + ToString(videoParam.reserved3) + "\n";
    str += structName + ".AsyncDepth=" + ToString(videoParam.AsyncDepth) + "\n";
    str += dump(structName + ".mfx", videoParam.mfx);
    str += dump(structName + ".vpp", videoParam.vpp) + "\n";
    str += structName + ".Protected=" + ToString(videoParam.Protected) + "\n";
    str += structName + ".IOPattern=" + GetIOPatternInString(videoParam.IOPattern) + "\n";
    str += dump_mfxExtParams(structName, videoParam);
    str += structName + ".reserved2=" + ToString(videoParam.reserved2);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxInfoMFX& mfx)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(mfx.reserved) + "\n";
    str += structName + ".LowPower=" + ToString(mfx.LowPower) + "\n";
    str += structName + ".BRCParamMultiplier=" + ToString(mfx.BRCParamMultiplier) + "\n";
    str += dump(structName + ".FrameInfo", mfx.FrameInfo) + "\n";
    str += structName + ".CodecId=" + GetCodecIdInString(mfx.CodecId) + "\n";
    str += structName + ".CodecProfile=" + ToString(mfx.CodecProfile) + "\n";
    str += structName + ".CodecLevel=" + ToString(mfx.CodecLevel) + "\n";
    str += structName + ".NumThread=" + ToString(mfx.NumThread) + "\n";
    str += structName + ".TargetUsage=" + ToString(mfx.TargetUsage) + "\n";
    str += structName + ".GopPicSize=" + ToString(mfx.GopPicSize) + "\n";
    str += structName + ".GopRefDist=" + ToString(mfx.GopRefDist) + "\n";
    str += structName + ".GopOptFlag=" + ToString(mfx.GopOptFlag) + "\n";
    str += structName + ".IdrInterval=" + ToString(mfx.IdrInterval) + "\n";
    str += structName + ".RateControlMethod=" + ToString(mfx.RateControlMethod) + "\n";
    str += structName + ".InitialDelayInKB=" + ToString(mfx.InitialDelayInKB) + "\n";
    str += structName + ".QPI=" + ToString(mfx.QPI) + "\n";
    str += structName + ".Accuracy=" + ToString(mfx.Accuracy) + "\n";
    str += structName + ".BufferSizeInKB=" + ToString(mfx.BufferSizeInKB) + "\n";
    str += structName + ".TargetKbps=" + ToString(mfx.TargetKbps) + "\n";
    str += structName + ".QPP=" + ToString(mfx.QPP) + "\n";
    str += structName + ".ICQQuality=" + ToString(mfx.ICQQuality) + "\n";
    str += structName + ".MaxKbps=" + ToString(mfx.MaxKbps) + "\n";
    str += structName + ".QPB=" + ToString(mfx.QPB) + "\n";
    str += structName + ".Convergence=" + ToString(mfx.Convergence) + "\n";
    str += structName + ".NumSlice=" + ToString(mfx.NumSlice) + "\n";
    str += structName + ".NumRefFrame=" + ToString(mfx.NumRefFrame) + "\n";
    str += structName + ".EncodedOrder=" + ToString(mfx.EncodedOrder) + "\n";
    str += structName + ".DecodedOrder=" + ToString(mfx.DecodedOrder) + "\n";
    str += structName + ".ExtendedPicStruct=" + ToString(mfx.ExtendedPicStruct) + "\n";
    str += structName + ".TimeStampCalc=" + ToString(mfx.TimeStampCalc) + "\n";
    str += structName + ".SliceGroupsPresent=" + ToString(mfx.SliceGroupsPresent) + "\n";
    str += structName + ".MaxDecFrameBuffering=" + ToString(mfx.MaxDecFrameBuffering) + "\n";
    str += structName + ".EnableReallocRequest=" + ToString(mfx.EnableReallocRequest) + "\n";
    str += structName + ".FilmGrain=" + ToString(mfx.FilmGrain) + "\n";
    str += structName + ".IgnoreLevelConstrain=" + ToString(mfx.IgnoreLevelConstrain) + "\n";
    str += structName + ".reserved2[]=" + DUMP_RESERVED_ARRAY(mfx.reserved2) + "\n";
    str += structName + ".JPEGChromaFormat=" + ToString(mfx.JPEGChromaFormat) + "\n";
    str += structName + ".Rotation=" + ToString(mfx.Rotation) + "\n";
    str += structName + ".JPEGColorFormat=" + ToString(mfx.JPEGColorFormat) + "\n";
    str += structName + ".InterleavedDec=" + ToString(mfx.InterleavedDec) + "\n";
    str += structName + ".reserved3[]=" + DUMP_RESERVED_ARRAY(mfx.reserved3) + "\n";
    str += structName + ".Interleaved=" + ToString(mfx.Interleaved) + "\n";
    str += structName + ".Quality=" + ToString(mfx.Quality) + "\n";
    str += structName + ".RestartInterval=" + ToString(mfx.RestartInterval) + "\n";
    str += structName + ".reserved5[]=" + DUMP_RESERVED_ARRAY(mfx.reserved5) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxInfoVPP& vpp)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(vpp.reserved) + "\n";
    str += dump(structName + ".In", vpp.In) + "\n" +
    str += dump(structName + ".Out", vpp.Out);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameAllocRequest& frameAllocRequest)
{
    std::string str = "mfxFrameAllocRequest " + structName + " : addr[" + ToHexFormatString(&frameAllocRequest) + "]" + " size[" + ToString(sizeof(frameAllocRequest)) + "]" + "\n";
    str += structName + ".AllocId=" + ToString(frameAllocRequest.AllocId) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(frameAllocRequest.reserved) + "\n";
    str += structName + ".reserved3[]=" + DUMP_RESERVED_ARRAY(frameAllocRequest.reserved3) + "\n";
    str += dump(structName + ".Info", frameAllocRequest.Info) + "\n";
    str += structName + ".Type=" + ToString(frameAllocRequest.Type) + "\n";
    str += structName + ".NumFrameMin=" + ToString(frameAllocRequest.NumFrameMin) + "\n";
    str += structName + ".NumFrameSuggested=" + ToString(frameAllocRequest.NumFrameSuggested) + "\n";
    str += structName + ".reserved2=" + ToString(frameAllocRequest.reserved2);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameInfo& info)
{
    std::string str;
    str += structName + ".reserved=" + DUMP_RESERVED_ARRAY(info.reserved) + "\n";
    str += structName + ".ChannelId=" + ToString(info.ChannelId) + "\n";
    str += structName + ".BitDepthLuma=" + ToString(info.BitDepthLuma) + "\n";
    str += structName + ".BitDepthChroma=" + ToString(info.BitDepthChroma) + "\n";
    str += structName + ".Shift=" + ToString(info.Shift) + "\n";
    str += dump(structName + ".mfxFrameId", info.FrameId) + "\n";
    str += structName + ".FourCC=" + GetFourCCInString(info.FourCC) + "\n";
    str += structName + ".Width=" + ToString(info.Width) + "\n";
    str += structName + ".Height=" + ToString(info.Height) + "\n";
    str += structName + ".CropX=" + ToString(info.CropX) + "\n";
    str += structName + ".CropY=" + ToString(info.CropY) + "\n";
    str += structName + ".CropW=" + ToString(info.CropW) + "\n";
    str += structName + ".CropH=" + ToString(info.CropH) + "\n";
    str += structName + ".BufferSize=" + ToString(info.BufferSize) + "\n";
    str += structName + ".reserved5=" + ToString(info.reserved5) + "\n";
    str += structName + ".FrameRateExtN=" + ToString(info.FrameRateExtN) + "\n";
    str += structName + ".FrameRateExtD=" + ToString(info.FrameRateExtD) + "\n";
    str += structName + ".reserved3=" + ToString(info.reserved3) + "\n";
    str += structName + ".AspectRatioW=" + ToString(info.AspectRatioW) + "\n";
    str += structName + ".AspectRatioH=" + ToString(info.AspectRatioH) + "\n";
    str += structName + ".PicStruct=" + ToString(info.PicStruct) + "\n";
    str += structName + ".ChromaFormat=" + ToString(info.ChromaFormat) + "\n";
    str += structName + ".reserved2=" + ToString(info.reserved2);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameId& frame)
{
    std::string str;
    str += structName + ".TemporalId=" + ToString(frame.TemporalId) + "\n";
    str += structName + ".PriorityId=" + ToString(frame.PriorityId) + "\n";
    str += structName + ".DependencyId=" + ToString(frame.DependencyId) + "\n";
    str += structName + ".QualityId=" + ToString(frame.QualityId) + "\n";
    str += structName + ".ViewId=" + ToString(frame.ViewId);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameSurface1& frameSurface1)
{
    std::string str = "mfxFrameSurface1 " + structName + " : addr[" + ToHexFormatString(&frameSurface1) + "]" + " size[" + ToString(sizeof(frameSurface1)) + "]" + "\n";
    str += structName + ".FrameInterface=" + ToHexFormatString(frameSurface1.FrameInterface) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(frameSurface1.reserved) + "\n";
    str += structName + ".reserved1[]=" + DUMP_RESERVED_ARRAY(frameSurface1.reserved1) + "\n";
    str += dump(structName + ".Info", frameSurface1.Info) + "\n";
    str += dump(structName + ".Data", frameSurface1.Data);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameData& frameData)
{
    std::string str;
    str += dump_mfxExtParams(structName, frameData);
    str += structName + ".MemType=" + ToString(frameData.MemType) + "\n";
    str += structName + ".PitchHigh=" + ToString(frameData.PitchHigh) + "\n";
    str += structName + ".TimeStamp=" + ToString(frameData.TimeStamp) + "\n";
    str += structName + ".FrameOrder=" + ToString(frameData.FrameOrder) + "\n";
    str += structName + ".Locked=" + ToString(frameData.Locked) + "\n";
    str += structName + ".Pitch=" + ToString(frameData.Pitch) + "\n";
    str += structName + ".PitchLow=" + ToString(frameData.PitchLow) + "\n";

    str += structName + ".Y=" + ToHexFormatString(frameData.Y) + "\n";
    str += structName + ".Y16=" + ToHexFormatString(frameData.Y16) + "\n";
    str += structName + ".R=" + ToHexFormatString(frameData.R) + "\n";

    str += structName + ".UV=" + ToHexFormatString(frameData.UV) + "\n";
    str += structName + ".VU=" + ToHexFormatString(frameData.VU) + "\n";
    str += structName + ".CbCr=" + ToHexFormatString(frameData.CbCr) + "\n";
    str += structName + ".CrCb=" + ToHexFormatString(frameData.CrCb) + "\n";
    str += structName + ".Cb=" + ToHexFormatString(frameData.Cb) + "\n";
    str += structName + ".U=" + ToHexFormatString(frameData.U) + "\n";
    str += structName + ".U16=" + ToHexFormatString(frameData.U16) + "\n";
    str += structName + ".G=" + ToHexFormatString(frameData.G) + "\n";
    str += structName + ".Y410=" + ToHexFormatString(frameData.Y410) + "\n";
    str += structName + ".Y416=" + ToHexFormatString(frameData.Y416) + "\n";

    str += structName + ".Cr=" + ToHexFormatString(frameData.Cr) + "\n";
    str += structName + ".V=" + ToHexFormatString(frameData.V) + "\n";
    str += structName + ".V16=" + ToHexFormatString(frameData.V16) + "\n";
    str += structName + ".B=" + ToHexFormatString(frameData.B) + "\n";
    str += structName + ".A2RGB10=" + ToString(frameData.A2RGB10) + "\n";
#ifdef ONEVPL_EXPERIMENTAL
    str += structName + ".ABGRFP16=" + ToString(frameData.ABGRFP16) + "\n";
#endif
    str += structName + ".A=" + ToHexFormatString(frameData.A) + "\n";
    str += structName + ".MemId=" + ToString(frameData.MemId) + "\n";
    str += structName + ".Corrupted=" + ToString(frameData.Corrupted) + "\n";
    str += structName + ".DataFlag=" + ToString(frameData.DataFlag) + "\n";
    
    return str;
}

//ExtendedBuffer
std::string DumpContext::dump(const std::string structName, const mfxExtCodingOption& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    str += structName + ".reserved1=" + ToString(_struct.reserved1) + "\n";
    str += structName + ".RateDistortionOpt=" + ToString(_struct.RateDistortionOpt) + "\n";
    str += structName + ".MECostType=" + ToString(_struct.MECostType) + "\n";
    str += structName + ".MESearchType=" + ToString(_struct.MESearchType) + "\n";
    str += structName + ".MVSearchWindow.x=" + ToString(_struct.MVSearchWindow.x) + "\n";
    str += structName + ".MVSearchWindow.y=" + ToString(_struct.MVSearchWindow.y) + "\n";
    str += structName + ".EndOfSequence=" + ToString(_struct.EndOfSequence) + "\n";
    str += structName + ".FramePicture=" + ToString(_struct.FramePicture) + "\n";
    str += structName + ".CAVLC=" + ToString(_struct.CAVLC) + "\n";
    str += structName + ".reserved2[]=" + DUMP_RESERVED_ARRAY(_struct.reserved2) + "\n";
    str += structName + ".RecoveryPointSEI=" + ToString(_struct.RecoveryPointSEI) + "\n";
    str += structName + ".ViewOutput=" + ToString(_struct.ViewOutput) + "\n";
    str += structName + ".NalHrdConformance=" + ToString(_struct.NalHrdConformance) + "\n";
    str += structName + ".SingleSeiNalUnit=" + ToString(_struct.SingleSeiNalUnit) + "\n";
    str += structName + ".VuiVclHrdParameters=" + ToString(_struct.VuiVclHrdParameters) + "\n";
    str += structName + ".RefPicListReordering=" + ToString(_struct.RefPicListReordering) + "\n";
    str += structName + ".ResetRefList=" + ToString(_struct.ResetRefList) + "\n";
    str += structName + ".RefPicMarkRep=" + ToString(_struct.RefPicMarkRep) + "\n";
    str += structName + ".FieldOutput=" + ToString(_struct.FieldOutput) + "\n";
    str += structName + ".IntraPredBlockSize=" + ToString(_struct.IntraPredBlockSize) + "\n";
    str += structName + ".InterPredBlockSize=" + ToString(_struct.InterPredBlockSize) + "\n";
    str += structName + ".MVPrecision=" + ToString(_struct.MVPrecision) + "\n";
    str += structName + ".MaxDecFrameBuffering=" + ToString(_struct.MaxDecFrameBuffering) + "\n";
    str += structName + ".AUDelimiter=" + ToString(_struct.AUDelimiter) + "\n";
    str += structName + ".EndOfStream=" + ToString(_struct.EndOfStream) + "\n";
    str += structName + ".PicTimingSEI=" + ToString(_struct.PicTimingSEI) + "\n";
    str += structName + ".VuiNalHrdParameters=" + ToString(_struct.VuiNalHrdParameters) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCodingOption2& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(IntRefType);
    DUMP_FIELD(IntRefCycleSize);
    DUMP_FIELD(IntRefQPDelta);
    DUMP_FIELD(MaxFrameSize);
    DUMP_FIELD(MaxSliceSize);
    DUMP_FIELD(BitrateLimit);
    DUMP_FIELD(MBBRC);
    DUMP_FIELD(ExtBRC);
    DUMP_FIELD(LookAheadDepth);
    DUMP_FIELD(Trellis);
    DUMP_FIELD(RepeatPPS);
    DUMP_FIELD(BRefType);
    DUMP_FIELD(AdaptiveI);
    DUMP_FIELD(AdaptiveB);
    DUMP_FIELD(LookAheadDS);
    DUMP_FIELD(NumMbPerSlice);
    DUMP_FIELD(SkipFrame);
    DUMP_FIELD_UCHAR(MinQPI);
    DUMP_FIELD_UCHAR(MaxQPI);
    DUMP_FIELD_UCHAR(MinQPP);
    DUMP_FIELD_UCHAR(MaxQPP);
    DUMP_FIELD_UCHAR(MinQPB);
    DUMP_FIELD_UCHAR(MaxQPB);
    DUMP_FIELD(FixedFrameRate);
    DUMP_FIELD(DisableDeblockingIdc);
    DUMP_FIELD(DisableVUI);
    DUMP_FIELD(BufferingPeriodSEI);
    DUMP_FIELD(EnableMAD);
    DUMP_FIELD(UseRawRef);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCodingOption3& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(NumSliceI);
    DUMP_FIELD(NumSliceP);
    DUMP_FIELD(NumSliceB);
    DUMP_FIELD(WinBRCMaxAvgKbps);
    DUMP_FIELD(WinBRCSize);
    DUMP_FIELD(QVBRQuality);
    DUMP_FIELD(EnableMBQP);
    DUMP_FIELD(IntRefCycleDist);
    DUMP_FIELD(DirectBiasAdjustment);
    DUMP_FIELD(GlobalMotionBiasAdjustment);
    DUMP_FIELD(MVCostScalingFactor);
    DUMP_FIELD(MBDisableSkipMap);
    DUMP_FIELD(WeightedPred);
    DUMP_FIELD(WeightedBiPred);
    DUMP_FIELD(AspectRatioInfoPresent);
    DUMP_FIELD(OverscanInfoPresent);
    DUMP_FIELD(OverscanAppropriate);
    DUMP_FIELD(TimingInfoPresent);
    DUMP_FIELD(BitstreamRestriction);
    DUMP_FIELD(LowDelayHrd);
    DUMP_FIELD(MotionVectorsOverPicBoundaries);
    DUMP_FIELD_RESERVED(reserved1);
    DUMP_FIELD(ScenarioInfo);
    DUMP_FIELD(ContentInfo);
    DUMP_FIELD(PRefType);
    DUMP_FIELD(FadeDetection);
    DUMP_FIELD(GPB);
    DUMP_FIELD(MaxFrameSizeI);
    DUMP_FIELD(MaxFrameSizeP);
    DUMP_FIELD(EnableQPOffset);
    DUMP_FIELD_RESERVED(QPOffset);
    DUMP_FIELD_RESERVED(NumRefActiveP);
    DUMP_FIELD_RESERVED(NumRefActiveBL0);
    DUMP_FIELD_RESERVED(NumRefActiveBL1);
    DUMP_FIELD(TransformSkip);
    DUMP_FIELD(TargetChromaFormatPlus1);
    DUMP_FIELD(TargetBitDepthLuma);
    DUMP_FIELD(TargetBitDepthChroma);
    DUMP_FIELD(BRCPanicMode);
    DUMP_FIELD(LowDelayBRC);
    DUMP_FIELD(EnableMBForceIntra);
    DUMP_FIELD(AdaptiveMaxFrameSize);
    DUMP_FIELD(RepartitionCheckEnable);
    DUMP_FIELD_RESERVED(reserved5);
    DUMP_FIELD(EncodedUnitsInfo);
    DUMP_FIELD(EnableNalUnitType);
    DUMP_FIELD(AdaptiveLTR);
    DUMP_FIELD(AdaptiveCQM);
    DUMP_FIELD(AdaptiveRef);
    DUMP_FIELD_RESERVED(reserved);

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtEncoderResetOption& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(StartNewSequence);
    DUMP_FIELD_RESERVED(reserved);
    return str;
}

std::string DumpContext::dump(const std::string structName, const LongTermRefList& longTermRefList)
{
    std::string str;
    str += structName + ".FrameOrder=" + ToString(longTermRefList.FrameOrder) + "\n";
    str += structName + ".PicStruct=" + ToString(longTermRefList.PicStruct) + "\n";
    str += structName + ".ViewId=" + ToString(longTermRefList.ViewId) + "\n";
    str += structName + ".LongTermIdx=" + ToString(longTermRefList.LongTermIdx) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(longTermRefList.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtAVCRefListCtrl& ExtAVCRefListCtrl)
{
    std::string str;
    str += dump(structName + ".Header", ExtAVCRefListCtrl.Header) + "\n";
    str += structName + ".NumRefIdxL0Active=" + ToString(ExtAVCRefListCtrl.NumRefIdxL0Active) + "\n";
    str += structName + ".NumRefIdxL1Active=" + ToString(ExtAVCRefListCtrl.NumRefIdxL1Active) + "\n";
    str += structName + ".PreferredRefList=" + ToString(ExtAVCRefListCtrl.PreferredRefList) + "\n";
    for (int i = 0; i < 32; i++)
    {
        if (ExtAVCRefListCtrl.PreferredRefList[i].FrameOrder == mfxU32(MFX_FRAMEORDER_UNKNOWN)
            && ExtAVCRefListCtrl.PreferredRefList[i].LongTermIdx == 0)
            continue;

        str += "i: " + ToString(i) + "\n";
        str += dump("PreferredRefList: ", *((LongTermRefList *)(&ExtAVCRefListCtrl.PreferredRefList[i]))) + "\n";
    }
    str += structName + ".RejectedRefList=" + ToString(ExtAVCRefListCtrl.RejectedRefList) + "\n";
    for (int i = 0; i < 16; i++)
    {
        if (ExtAVCRefListCtrl.RejectedRefList[i].FrameOrder == mfxU32(MFX_FRAMEORDER_UNKNOWN)
           && ExtAVCRefListCtrl.RejectedRefList[i].LongTermIdx == 0)
            continue;

        str += "i: " + ToString(i) + "\n";
        str += dump("RejectedRefList: ", *((LongTermRefList *)(&ExtAVCRefListCtrl.RejectedRefList[i]))) + "\n";
    }
    str += structName + ".LongTermRefList=" + ToString(ExtAVCRefListCtrl.LongTermRefList) + "\n";
    for (int i = 0; i < 16; i++)
    {
        if (ExtAVCRefListCtrl.LongTermRefList[i].FrameOrder == mfxU32(MFX_FRAMEORDER_UNKNOWN)
            && ExtAVCRefListCtrl.LongTermRefList[i].LongTermIdx == 0)
            continue;

        str += "i: " + ToString(i) + "\n";
        str += dump("LongTermRefList: ", *((LongTermRefList *)(&ExtAVCRefListCtrl.LongTermRefList[i]))) + "\n";
    }
    str += structName + ".ApplyLongTermIdx=" + ToString(ExtAVCRefListCtrl.ApplyLongTermIdx) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtAVCRefListCtrl.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtAvcTemporalLayers& ExtAvcTemporalLayers)
{
    std::string str;
    str += dump(structName + ".Header", ExtAvcTemporalLayers.Header) + "\n";
    str += structName + ".reserved1[]=" + DUMP_RESERVED_ARRAY(ExtAvcTemporalLayers.reserved1) + "\n";
    str += structName + ".reserved2=" + ToString(ExtAvcTemporalLayers.reserved2) + "\n";
    str += structName + ".BaseLayerPID=" + ToString(ExtAvcTemporalLayers.BaseLayerPID) + "\n";
    str += structName + ".Layer=" + ToString(ExtAvcTemporalLayers.Layer) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtAVCEncodedFrameInfo& ExtAVCEncodedFrameInfo)
{
    std::string str;
    str += dump(structName + ".Header", ExtAVCEncodedFrameInfo.Header) + "\n";
    str += structName + ".FrameOrder=" + ToString(ExtAVCEncodedFrameInfo.FrameOrder) + "\n";
    str += structName + ".PicStruct=" + ToString(ExtAVCEncodedFrameInfo.PicStruct) + "\n";
    str += structName + ".LongTermIdx=" + ToString(ExtAVCEncodedFrameInfo.LongTermIdx) + "\n";
    str += structName + ".MAD=" + ToString(ExtAVCEncodedFrameInfo.MAD) + "\n";
    str += structName + ".BRCPanicMode=" + ToString(ExtAVCEncodedFrameInfo.BRCPanicMode) + "\n";
    str += structName + ".QP=" + ToString(ExtAVCEncodedFrameInfo.QP) + "\n";
    str += structName + ".SecondFieldOffset=" + ToString(ExtAVCEncodedFrameInfo.SecondFieldOffset) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtAVCEncodedFrameInfo.reserved) + "\n";
    str += structName + ".UsedRefListL0=" + ToHexFormatString(ExtAVCEncodedFrameInfo.UsedRefListL0) + "\n";
    str += structName + ".UsedRefListL1=" + ToHexFormatString(ExtAVCEncodedFrameInfo.UsedRefListL1) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtAVCRefLists& ExtAVCRefLists)
{
    std::string str;
    str += dump(structName + ".Header", ExtAVCRefLists.Header) + "\n";
    str += structName + ".NumRefIdxL0Active=" + ToString(ExtAVCRefLists.NumRefIdxL0Active) + "\n";
    str += structName + ".NumRefIdxL1Active=" + ToString(ExtAVCRefLists.NumRefIdxL1Active) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtAVCRefLists.reserved) + "\n";
    str += structName + ".RefPicList0=" + ToString(ExtAVCRefLists.RefPicList0) + "\n";
    str += structName + ".RefPicList1=" + ToString(ExtAVCRefLists.RefPicList1) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPDenoise& ExtVPPDenoise)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPDenoise.Header) + "\n";
    str += structName + ".DenoiseFactor=" + ToString(ExtVPPDenoise.DenoiseFactor) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPDetail& ExtVPPDetail)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPDetail.Header) + "\n";
    str += structName + ".DetailFactor=" + ToString(ExtVPPDetail.DetailFactor) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPProcAmp& ExtVPPProcAmp)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPProcAmp.Header) + "\n";
    str += structName + ".Brightness=" + ToString(ExtVPPProcAmp.Brightness) + "\n";
    str += structName + ".Contrast=" + ToString(ExtVPPProcAmp.Contrast) + "\n";
    str += structName + ".Hue=" + ToString(ExtVPPProcAmp.Hue) + "\n";
    str += structName + ".Saturation=" + ToString(ExtVPPProcAmp.Saturation) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCodingOptionSPSPPS& ExtCodingOptionSPSPPS)
{
    std::string str;
    str += dump(structName + ".Header", ExtCodingOptionSPSPPS.Header) + "\n";
    str += structName + ".SPSBuffer=" + ToHexFormatString(ExtCodingOptionSPSPPS.SPSBuffer) + "\n";
    str += structName + ".PPSBuffer=" + ToHexFormatString(ExtCodingOptionSPSPPS.PPSBuffer) + "\n";
    str += structName + ".SPSBufSize=" + ToString(ExtCodingOptionSPSPPS.SPSBufSize) + "\n";
    str += structName + ".PPSBufSize=" + ToString(ExtCodingOptionSPSPPS.PPSBufSize) + "\n";
    str += structName + ".SPSId=" + ToString(ExtCodingOptionSPSPPS.SPSId) + "\n";
    str += structName + ".PPSId=" + ToString(ExtCodingOptionSPSPPS.PPSId) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVideoSignalInfo& ExtVideoSignalInfo)
{
    std::string str;
    str += dump(structName + ".Header", ExtVideoSignalInfo.Header) + "\n";
    str += structName + ".VideoFormat=" + ToString(ExtVideoSignalInfo.VideoFormat) + "\n";
    str += structName + ".VideoFullRange=" + ToString(ExtVideoSignalInfo.VideoFullRange) + "\n";
    str += structName + ".ColourDescriptionPresent=" + ToString(ExtVideoSignalInfo.ColourDescriptionPresent) + "\n";
    str += structName + ".ColourPrimaries=" + ToString(ExtVideoSignalInfo.ColourPrimaries) + "\n";
    str += structName + ".TransferCharacteristics=" + ToString(ExtVideoSignalInfo.TransferCharacteristics) + "\n";
    str += structName + ".MatrixCoefficients=" + ToString(ExtVideoSignalInfo.MatrixCoefficients) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPDoUse& ExtVPPDoUse)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPDoUse.Header) + "\n";
    str += structName + ".NumAlg=" + ToString(ExtVPPDoUse.NumAlg) + "\n";
    str += structName + ".AlgList=" + ToHexFormatString(ExtVPPDoUse.AlgList) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtPictureTimingSEI& ExtPictureTimingSEI)
{
    std::string str;
    str += dump(structName + ".Header", ExtPictureTimingSEI.Header) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtPictureTimingSEI.reserved) + "\n";
    str += structName + ".TimeStamp[0].ClockTimestampFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[0].ClockTimestampFlag) + "\n";
    str += structName + ".TimeStamp[0].CtType=" + ToString(ExtPictureTimingSEI.TimeStamp[0].CtType) + "\n";
    str += structName + ".TimeStamp[0].NuitFieldBasedFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[0].NuitFieldBasedFlag) + "\n";
    str += structName + ".TimeStamp[0].CountingType=" + ToString(ExtPictureTimingSEI.TimeStamp[0].CountingType) + "\n";
    str += structName + ".TimeStamp[0].FullTimestampFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[0].FullTimestampFlag) + "\n";
    str += structName + ".TimeStamp[0].DiscontinuityFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[0].DiscontinuityFlag) + "\n";
    str += structName + ".TimeStamp[0].CntDroppedFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[0].CntDroppedFlag) + "\n";
    str += structName + ".TimeStamp[0].NFrames=" + ToString(ExtPictureTimingSEI.TimeStamp[0].NFrames) + "\n";
    str += structName + ".TimeStamp[0].SecondsFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[0].SecondsFlag) + "\n";
    str += structName + ".TimeStamp[0].MinutesFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[0].MinutesFlag) + "\n";
    str += structName + ".TimeStamp[0].HoursFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[0].HoursFlag) + "\n";
    str += structName + ".TimeStamp[0].SecondsValue=" + ToString(ExtPictureTimingSEI.TimeStamp[0].SecondsValue) + "\n";
    str += structName + ".TimeStamp[0].MinutesValue=" + ToString(ExtPictureTimingSEI.TimeStamp[0].MinutesValue) + "\n";
    str += structName + ".TimeStamp[0].HoursValue=" + ToString(ExtPictureTimingSEI.TimeStamp[0].HoursValue) + "\n";
    str += structName + ".TimeStamp[0].TimeOffset=" + ToString(ExtPictureTimingSEI.TimeStamp[0].TimeOffset) + "\n";
    str += structName + ".TimeStamp[1].ClockTimestampFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[1].ClockTimestampFlag) + "\n";
    str += structName + ".TimeStamp[1].CtType=" + ToString(ExtPictureTimingSEI.TimeStamp[1].CtType) + "\n";
    str += structName + ".TimeStamp[1].NuitFieldBasedFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[1].NuitFieldBasedFlag) + "\n";
    str += structName + ".TimeStamp[1].CountingType=" + ToString(ExtPictureTimingSEI.TimeStamp[1].CountingType) + "\n";
    str += structName + ".TimeStamp[1].FullTimestampFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[1].FullTimestampFlag) + "\n";
    str += structName + ".TimeStamp[1].DiscontinuityFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[1].DiscontinuityFlag) + "\n";
    str += structName + ".TimeStamp[1].CntDroppedFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[1].CntDroppedFlag) + "\n";
    str += structName + ".TimeStamp[1].NFrames=" + ToString(ExtPictureTimingSEI.TimeStamp[1].NFrames) + "\n";
    str += structName + ".TimeStamp[1].SecondsFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[1].SecondsFlag) + "\n";
    str += structName + ".TimeStamp[1].MinutesFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[1].MinutesFlag) + "\n";
    str += structName + ".TimeStamp[1].HoursFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[1].HoursFlag) + "\n";
    str += structName + ".TimeStamp[1].SecondsValue=" + ToString(ExtPictureTimingSEI.TimeStamp[1].SecondsValue) + "\n";
    str += structName + ".TimeStamp[1].MinutesValue=" + ToString(ExtPictureTimingSEI.TimeStamp[1].MinutesValue) + "\n";
    str += structName + ".TimeStamp[1].HoursValue=" + ToString(ExtPictureTimingSEI.TimeStamp[1].HoursValue) + "\n";
    str += structName + ".TimeStamp[1].TimeOffset=" + ToString(ExtPictureTimingSEI.TimeStamp[1].TimeOffset) + "\n";
    str += structName + ".TimeStamp[2].ClockTimestampFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[2].ClockTimestampFlag) + "\n";
    str += structName + ".TimeStamp[2].CtType=" + ToString(ExtPictureTimingSEI.TimeStamp[2].CtType) + "\n";
    str += structName + ".TimeStamp[2].NuitFieldBasedFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[2].NuitFieldBasedFlag) + "\n";
    str += structName + ".TimeStamp[2].CountingType=" + ToString(ExtPictureTimingSEI.TimeStamp[2].CountingType) + "\n";
    str += structName + ".TimeStamp[2].FullTimestampFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[2].FullTimestampFlag) + "\n";
    str += structName + ".TimeStamp[2].DiscontinuityFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[2].DiscontinuityFlag) + "\n";
    str += structName + ".TimeStamp[2].CntDroppedFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[2].CntDroppedFlag) + "\n";
    str += structName + ".TimeStamp[2].NFrames=" + ToString(ExtPictureTimingSEI.TimeStamp[2].NFrames) + "\n";
    str += structName + ".TimeStamp[2].SecondsFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[2].SecondsFlag) + "\n";
    str += structName + ".TimeStamp[2].MinutesFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[2].MinutesFlag) + "\n";
    str += structName + ".TimeStamp[2].HoursFlag=" + ToString(ExtPictureTimingSEI.TimeStamp[2].HoursFlag) + "\n";
    str += structName + ".TimeStamp[2].SecondsValue=" + ToString(ExtPictureTimingSEI.TimeStamp[2].SecondsValue) + "\n";
    str += structName + ".TimeStamp[2].MinutesValue=" + ToString(ExtPictureTimingSEI.TimeStamp[2].MinutesValue) + "\n";
    str += structName + ".TimeStamp[2].HoursValue=" + ToString(ExtPictureTimingSEI.TimeStamp[2].HoursValue) + "\n";
    str += structName + ".TimeStamp[2].TimeOffset=" + ToString(ExtPictureTimingSEI.TimeStamp[2].TimeOffset) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPComposite& ExtVPPComposite)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPComposite.Header) + "\n";
    str += structName + ".Y=" + ToString(ExtVPPComposite.Y) + "\n";
    str += structName + ".R=" + ToString(ExtVPPComposite.R) + "\n";
    str += structName + ".U=" + ToString(ExtVPPComposite.U) + "\n";
    str += structName + ".G=" + ToString(ExtVPPComposite.G) + "\n";
    str += structName + ".V=" + ToString(ExtVPPComposite.V) + "\n";
    str += structName + ".B=" + ToString(ExtVPPComposite.B) + "\n";
    str += structName + ".NumTiles=" + ToString(ExtVPPComposite.NumTiles) + "\n";
    str += structName + ".reserved1[]=" + DUMP_RESERVED_ARRAY(ExtVPPComposite.reserved1) + "\n";
    str += structName + ".NumInputStream=" + ToString(ExtVPPComposite.NumInputStream) + "\n";
    str += structName + ".InputStream=" + ToHexFormatString(ExtVPPComposite.InputStream) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPVideoSignalInfo& ExtVPPVideoSignalInfo)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPVideoSignalInfo.Header) + "\n";
    str += structName + ".reserved1[]=" + DUMP_RESERVED_ARRAY(ExtVPPVideoSignalInfo.reserved1) + "\n";
    str += structName + ".In.TransferMatrix=" + ToString(ExtVPPVideoSignalInfo.In.TransferMatrix) + "\n";
    str += structName + ".In.NominalRange=" + ToString(ExtVPPVideoSignalInfo.In.NominalRange) + "\n";
    str += structName + ".In.reserved2[]=" + DUMP_RESERVED_ARRAY(ExtVPPVideoSignalInfo.In.reserved2) + "\n";
    str += structName + ".Out.TransferMatrix=" + ToString(ExtVPPVideoSignalInfo.Out.TransferMatrix) + "\n";
    str += structName + ".Out.NominalRange=" + ToString(ExtVPPVideoSignalInfo.Out.NominalRange) + "\n";
    str += structName + ".Out.reserved2[]=" + DUMP_RESERVED_ARRAY(ExtVPPVideoSignalInfo.Out.reserved2) + "\n";
    str += structName + ".TransferMatrix=" + ToString(ExtVPPVideoSignalInfo.TransferMatrix) + "\n";
    str += structName + ".NominalRange=" + ToString(ExtVPPVideoSignalInfo.NominalRange) + "\n";
    str += structName + ".reserved3[]=" + DUMP_RESERVED_ARRAY(ExtVPPVideoSignalInfo.reserved3) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPDeinterlacing& ExtVPPDeinterlacing)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPDeinterlacing.Header) + "\n";
    str += structName + ".Mode=" + ToString(ExtVPPDeinterlacing.Mode) + "\n";
    str += structName + ".TelecinePattern=" + ToString(ExtVPPDeinterlacing.TelecinePattern) + "\n";
    str += structName + ".TelecineLocation=" + ToString(ExtVPPDeinterlacing.TelecineLocation) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtVPPDeinterlacing.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtHEVCTiles& ExtHEVCTiles)
{
    std::string str;
    str += dump(structName + ".Header", ExtHEVCTiles.Header) + "\n";
    str += structName + ".NumTileRows=" + ToString(ExtHEVCTiles.NumTileRows) + "\n";
    str += structName + ".NumTileColumns=" + ToString(ExtHEVCTiles.NumTileColumns) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtHEVCTiles.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtHEVCParam& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";

    DUMP_FIELD(PicWidthInLumaSamples);
    DUMP_FIELD(PicHeightInLumaSamples);
    DUMP_FIELD(GeneralConstraintFlags);
    DUMP_FIELD(SampleAdaptiveOffset);
    DUMP_FIELD(LCUSize);
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(_struct.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtHEVCRegion& ExtHEVCRegion)
{
    std::string str;
    str += dump(structName + ".Header", ExtHEVCRegion.Header) + "\n";
    str += structName + ".RegionId=" + ToString(ExtHEVCRegion.RegionId) + "\n";
    str += structName + ".RegionType=" + ToString(ExtHEVCRegion.RegionType) + "\n";
    str += structName + ".RegionEncoding=" + ToString(ExtHEVCRegion.RegionEncoding) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtHEVCRegion.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtDecodedFrameInfo& ExtDecodedFrameInfo)
{
    std::string str;
    str += dump(structName + ".Header", ExtDecodedFrameInfo.Header) + "\n";
    str += structName + ".FrameType=" + ToString(ExtDecodedFrameInfo.FrameType) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtDecodedFrameInfo.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtTimeCode& ExtTimeCode)
{
    std::string str;
    str += dump(structName + ".Header", ExtTimeCode.Header) + "\n";
    str += structName + ".DropFrameFlag=" + ToString(ExtTimeCode.DropFrameFlag) + "\n";
    str += structName + ".TimeCodeHours=" + ToString(ExtTimeCode.TimeCodeHours) + "\n";
    str += structName + ".TimeCodeMinutes=" + ToString(ExtTimeCode.TimeCodeMinutes) + "\n";
    str += structName + ".TimeCodeSeconds=" + ToString(ExtTimeCode.TimeCodeSeconds) + "\n";
    str += structName + ".TimeCodePictures=" + ToString(ExtTimeCode.TimeCodePictures) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtTimeCode.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtPredWeightTable& ExtPredWeightTable)
{
    std::string str;
    str += dump(structName + ".Header", ExtPredWeightTable.Header) + "\n";
    str += structName + ".LumaLog2WeightDenom=" + ToString(ExtPredWeightTable.LumaLog2WeightDenom) + "\n";
    str += structName + ".ChromaLog2WeightDenom=" + ToString(ExtPredWeightTable.ChromaLog2WeightDenom) + "\n";
    for (int i = 0; i < 2; i++)
        str += structName + ".LumaWeightFlag[" + ToString(i) + "][]=" + DUMP_RESERVED_ARRAY(ExtPredWeightTable.LumaWeightFlag[i]) + "\n";
    for (int i = 0; i < 2; i++)
        str += structName + ".ChromaWeightFlag[" + ToString(i) + "][]=" + DUMP_RESERVED_ARRAY(ExtPredWeightTable.ChromaWeightFlag[i]) + "\n";
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 32; j++)
            for (int k = 0; k < 3; k++)
                str += structName + ".Weights[" + ToString(i) + "][" + ToString(j) + "][" + ToString(k) + "][]=" + DUMP_RESERVED_ARRAY(ExtPredWeightTable.Weights[i][j][k]) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtPredWeightTable.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtEncoderCapability& ExtEncoderCapability)
{
    std::string str;
    str += dump(structName + ".Header", ExtEncoderCapability.Header) + "\n";
    str += structName + ".MBPerSec=" + ToString(ExtEncoderCapability.MBPerSec) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtEncoderCapability.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtDirtyRect& ExtDirtyRect)
{
    std::string str;
    str += dump(structName + ".Header", ExtDirtyRect.Header) + "\n";
    str += structName + ".NumRect=" + ToString(ExtDirtyRect.NumRect) + "\n";
    str += structName + ".reserved1[]=" + DUMP_RESERVED_ARRAY(ExtDirtyRect.reserved1) + "\n";
    str += structName + ".Rect=" + ToString(ExtDirtyRect.Rect) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtMoveRect& ExtMoveRect)
{
    std::string str;
    str += dump(structName + ".Header", ExtMoveRect.Header) + "\n";
    str += structName + ".NumRect=" + ToString(ExtMoveRect.NumRect) + "\n";
    str += structName + ".reserved1[]=" + DUMP_RESERVED_ARRAY(ExtMoveRect.reserved1) + "\n";
    str += structName + ".Rect=" + ToString(ExtMoveRect.Rect) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPFrameRateConversion& ExtVPPFrameRateConversion)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPFrameRateConversion.Header) + "\n";
    str += structName + ".Algorithm=" + ToString(ExtVPPFrameRateConversion.Algorithm) + "\n";
    str += structName + ".reserved=" + ToString(ExtVPPFrameRateConversion.reserved) + "\n";
    str += structName + ".reserved2[]=" + DUMP_RESERVED_ARRAY(ExtVPPFrameRateConversion.reserved2) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPImageStab& ExtVPPImageStab)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPImageStab.Header) + "\n";
    str += structName + ".Mode=" + ToString(ExtVPPImageStab.Mode) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtVPPImageStab.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtEncoderROI& ExtEncoderROI)
{
    std::string str;
    str += dump(structName + ".Header", ExtEncoderROI.Header) + "\n";
    str += structName + ".NumROI=" + ToString(ExtEncoderROI.NumROI) + "\n";
    str += structName + ".ROIMode=" + ToString(ExtEncoderROI.ROIMode) + "\n";
    for (int i = 0; i < ExtEncoderROI.NumROI; i++) {
        str += structName + ".ROI[" + ToString(i) + "].Left=" + ToString(ExtEncoderROI.ROI[i].Left) + "\n";
        str += structName + ".ROI[" + ToString(i) + "].Top=" + ToString(ExtEncoderROI.ROI[i].Top) + "\n";
        str += structName + ".ROI[" + ToString(i) + "].Right=" + ToString(ExtEncoderROI.ROI[i].Right) + "\n";
        str += structName + ".ROI[" + ToString(i) + "].Bottom=" + ToString(ExtEncoderROI.ROI[i].Bottom) + "\n";
        if (ExtEncoderROI.ROIMode == MFX_ROI_MODE_QP_DELTA) {
            str += structName + ".ROI[" + ToString(i) + "].DeltaQP=" + ToString(ExtEncoderROI.ROI[i].DeltaQP) + "\n";
        }
        else {
            str += structName + ".ROI[" + ToString(i) + "].Priority=" + ToString(ExtEncoderROI.ROI[i].Priority) + "\n";
        }
        str += structName + ".ROI[" + ToString(i) + "].reserved2[]=" + DUMP_RESERVED_ARRAY(ExtEncoderROI.ROI[i].reserved2) + "\n";
    }
    str += structName + ".reserved1[]=" + DUMP_RESERVED_ARRAY(ExtEncoderROI.reserved1) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCodingOptionVPS& ExtCodingOptionVPS)
{
    std::string str;
    str += dump(structName + ".Header", ExtCodingOptionVPS.Header) + "\n";
    str += structName + ".VPSBuffer=" + ToHexFormatString(ExtCodingOptionVPS.VPSBuffer) + "\n";
    str += structName + ".reserved1=" + ToString(ExtCodingOptionVPS.reserved1) + "\n";
    str += structName + ".VPSBufSize=" + ToString(ExtCodingOptionVPS.VPSBufSize) + "\n";
    str += structName + ".VPSId=" + ToString(ExtCodingOptionVPS.VPSId) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtCodingOptionVPS.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtVPPRotation& ExtVPPRotation)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPRotation.Header) + "\n";
    str += structName + "Angle.=" + ToString(ExtVPPRotation.Angle) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtVPPRotation.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtEncodedSlicesInfo& ExtEncodedSlicesInfo)
{
    std::string str;
    str += dump(structName + ".Header", ExtEncodedSlicesInfo.Header) + "\n";
    str += structName + "SliceSizeOverflow.=" + ToString(ExtEncodedSlicesInfo.SliceSizeOverflow) + "\n";
    str += structName + "NumSliceNonCopliant.=" + ToString(ExtEncodedSlicesInfo.NumSliceNonCopliant) + "\n";
    str += structName + "NumEncodedSlice.=" + ToString(ExtEncodedSlicesInfo.NumEncodedSlice) + "\n";
    str += structName + "NumSliceSizeAlloc.=" + ToString(ExtEncodedSlicesInfo.NumSliceSizeAlloc) + "\n";
    str += structName + "SliceSize.=" + ToHexFormatString(ExtEncodedSlicesInfo.SliceSize) + "\n";
    str += structName + "reserved1.=" + ToString(ExtEncodedSlicesInfo.reserved1) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtEncodedSlicesInfo.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtVPPScaling& ExtVPPScaling)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPScaling.Header) + "\n";
    str += structName + "ScalingMode.=" + ToString(ExtVPPScaling.ScalingMode) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtVPPScaling.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPMirroring& ExtVPPMirroring)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPMirroring.Header) + "\n";
    str += structName + ".Type=" + ToString(ExtVPPMirroring.Type) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtVPPMirroring.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtMVOverPicBoundaries& ExtMVOverPicBoundaries)
{
    std::string str;
    str += dump(structName + ".Header", ExtMVOverPicBoundaries.Header) + "\n";
    str += structName + ".StickTop=" + ToString(ExtMVOverPicBoundaries.StickTop) + "\n";
    str += structName + ".StickBottom=" + ToString(ExtMVOverPicBoundaries.StickBottom) + "\n";
    str += structName + ".StickLeft=" + ToString(ExtMVOverPicBoundaries.StickLeft) + "\n";
    str += structName + ".StickRight=" + ToString(ExtMVOverPicBoundaries.StickRight) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtMVOverPicBoundaries.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPColorFill& ExtVPPColorFill)
{
    std::string str;
    str += dump(structName + ".Header", ExtVPPColorFill.Header) + "\n";
    str += structName + ".Enable=" + ToString(ExtVPPColorFill.Enable) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtVPPColorFill.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtDecVideoProcessing& ExtDecVideoProcessing)
{
    std::string str;
    /* IN structures */
    str += dump(structName + ".Header", ExtDecVideoProcessing.Header) + "\n";
    str += structName + "In.CropX=" + ToString(ExtDecVideoProcessing.In.CropX) + "\n";
    str += structName + "In.CropY=" + ToString(ExtDecVideoProcessing.In.CropY) + "\n";
    str += structName + "In.CropW=" + ToString(ExtDecVideoProcessing.In.CropW) + "\n";
    str += structName + "In.CropH=" + ToString(ExtDecVideoProcessing.In.CropH) + "\n";
    str += structName + "In.reserved[]=" + DUMP_RESERVED_ARRAY(ExtDecVideoProcessing.In.reserved) + "\n";
    /* Out structures */
    str += structName + "Out.FourCC=" + ToString(ExtDecVideoProcessing.Out.FourCC) + "\n";
    str += structName + "Out.ChromaFormat=" + ToString(ExtDecVideoProcessing.Out.ChromaFormat) + "\n";
    str += structName + "Out.reserved1=" + ToString(ExtDecVideoProcessing.Out.reserved1) + "\n";
    str += structName + "Out.Width=" + ToString(ExtDecVideoProcessing.Out.Width) + "\n";
    str += structName + "Out.Height=" + ToString(ExtDecVideoProcessing.Out.Height) + "\n";
    str += structName + "OutCropX.=" + ToString(ExtDecVideoProcessing.Out.CropX) + "\n";
    str += structName + "OutCropY.=" + ToString(ExtDecVideoProcessing.Out.CropY) + "\n";
    str += structName + "OutCropW.=" + ToString(ExtDecVideoProcessing.Out.CropW) + "\n";
    str += structName + "OutCropH.=" + ToString(ExtDecVideoProcessing.Out.CropH) + "\n";
    str += structName + "Out.reserved[]=" + DUMP_RESERVED_ARRAY(ExtDecVideoProcessing.Out.reserved) + "\n";

    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtDecVideoProcessing.reserved) + "\n";

    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtMBQP& ExtMBQP)
{
    std::string str;
    str += dump(structName + ".Header", ExtMBQP.Header) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtMBQP.reserved) + "\n";
    str += structName + ".Mode=" + ToString(ExtMBQP.Mode) + "\n";
    str += structName + ".BlockSize=" + ToString(ExtMBQP.BlockSize) + "\n";
    str += structName + ".NumQPAlloc=" + ToString(ExtMBQP.NumQPAlloc) + "\n";
    str += structName + ".QP=" + ToHexFormatString(ExtMBQP.QP) + "\n";
    str += structName + ".DeltaQP=" + ToHexFormatString(ExtMBQP.DeltaQP) + "\n";
    str += structName + "reserverd2=" + ToString(ExtMBQP.reserved2) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtEncoderIPCMArea& ExtEncoderIPCMArea)
{
    std::string str;
    str += dump(structName + ".Header", ExtEncoderIPCMArea.Header) + "\n";
    str += structName + ".reserve1[]=" + DUMP_RESERVED_ARRAY(ExtEncoderIPCMArea.reserve1) + "\n";
    str += structName + ".NumArea=" + ToString(ExtEncoderIPCMArea.NumArea) + "\n";
    // dump Area
    if (ExtEncoderIPCMArea.Areas == nullptr)
    {
        str += structName + ".Areas = nullptr \n";
        return str;
    }

    for (mfxU16 i = 0; i < ExtEncoderIPCMArea.NumArea; i++) {
        str += structName + ".Areas[" + ToString(i) + "].Left=" + ToString(ExtEncoderIPCMArea.Areas[i].Left) + "\n";
        str += structName + ".Areas[" + ToString(i) + "].Top=" + ToString(ExtEncoderIPCMArea.Areas[i].Top) + "\n";
        str += structName + ".Areas[" + ToString(i) + "].Right=" + ToString(ExtEncoderIPCMArea.Areas[i].Right) + "\n";
        str += structName + ".Areas[" + ToString(i) + "].Bottom=" + ToString(ExtEncoderIPCMArea.Areas[i].Bottom) + "\n";
        str += structName + ".Areas[" + ToString(i) + "].reserved2=" + DUMP_RESERVED_ARRAY(ExtEncoderIPCMArea.Areas[i].reserved2) + "\n";
    }
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtInsertHeaders& ExtInsertHeaders)
{
    std::string str;
    str += dump(structName + ".Header", ExtInsertHeaders.Header) + "\n";
    str += structName + ".SPS=" + ToString(ExtInsertHeaders.SPS) + "\n";
    str += structName + ".PPS=" + ToString(ExtInsertHeaders.PPS) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtDecodeErrorReport& ExtDecodeErrorReport)
{
    std::string str;
    str += dump(structName + ".Header", ExtDecodeErrorReport.Header) + "\n";
    str += structName + ".ErrorTypes=" + toString(ExtDecodeErrorReport.ErrorTypes, DUMP_HEX) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtDecodeErrorReport.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtMasteringDisplayColourVolume& _struct) {
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    str += structName + ".InsertPayloadToggle=" + ToString(_struct.InsertPayloadToggle) + "\n";
    for (int i = 0; i < 3; i++) {
        str += structName + ".DisplayPrimariesX[" + ToString(i) + "]=" + ToString(_struct.DisplayPrimariesX[i]) + "\n";
        str += structName + ".DisplayPrimariesY[" + ToString(i) + "]=" + ToString(_struct.DisplayPrimariesY[i]) + "\n";
    }
    str += structName + ".WhitePointX=" + ToString(_struct.WhitePointX) + "\n";
    str += structName + ".WhitePointY=" + ToString(_struct.WhitePointY) + "\n";
    str += structName + ".MaxDisplayMasteringLuminance=" + ToString(_struct.MaxDisplayMasteringLuminance) + "\n";
    str += structName + ".MinDisplayMasteringLuminance=" + ToString(_struct.MinDisplayMasteringLuminance) + "\n";

    DUMP_FIELD_RESERVED(reserved);
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtContentLightLevelInfo& _struct) {
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    str += structName + ".InsertPayloadToggle=" + ToString(_struct.InsertPayloadToggle) + "\n";
    str += structName + ".MaxContentLightLevel=" + ToString(_struct.MaxContentLightLevel) + "\n";
    str += structName + ".MaxPicAverageLightLevel=" + ToString(_struct.MaxPicAverageLightLevel) + "\n";

    DUMP_FIELD_RESERVED(reserved);
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtEncodedUnitsInfo& _struct) {
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    str += structName + ".NumUnitsAlloc=" + ToString(_struct.NumUnitsAlloc) + "\n";
    str += structName + ".NumUnitsEncoded=" + ToString(_struct.NumUnitsEncoded) + "\n";

    if (_struct.UnitInfo != NULL)
    {
        int count = _struct.NumUnitsEncoded < _struct.NumUnitsAlloc ? _struct.NumUnitsEncoded : _struct.NumUnitsAlloc;
        for (int i = 0; i < count; i++)
        {
            str += structName + ".UnitInfo[" + ToString(i) + "].Type=" + ToString(_struct.UnitInfo[i].Type) + "\n";
            str += structName + ".UnitInfo[" + ToString(i) + "].Offset=" + ToString(_struct.UnitInfo[i].Offset) + "\n";
            str += structName + ".UnitInfo[" + ToString(i) + "].Size=" + ToString(_struct.UnitInfo[i].Size) + "\n";
        }
    }
    else
    {
        str += structName + ".UnitInfo=NULL\n";
    }

    DUMP_FIELD_RESERVED(reserved);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtColorConversion& ExtColorConversion) {
    std::string str;
    str += dump(structName + ".Header", ExtColorConversion.Header) + "\n";
    str += structName + ".ChromaSiting=" + ToString(ExtColorConversion.ChromaSiting) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtColorConversion.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtVppMctf& _struct) {
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    str += structName + ".FilterStrength=" + ToString(_struct.FilterStrength) + "\n";
    DUMP_FIELD_RESERVED(reserved);
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtVP9Segmentation& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(NumSegments);

    for (mfxU16 i = 0; i < 8 && i < _struct.NumSegments; i++)
    {
        str += dump(structName + ".Segment[" + ToString(i) + "]", _struct.Segment[i]) + "\n";
    }

    DUMP_FIELD(SegmentIdBlockSize);
    DUMP_FIELD(NumSegmentIdAlloc);

    if (_struct.SegmentId)
    {
        str += dump_array_with_cast<mfxU8, mfxU16>(_struct.SegmentId, _struct.NumSegmentIdAlloc);
    }

    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxVP9SegmentParam& _struct)
{
    std::string str;
    DUMP_FIELD(FeatureEnabled);
    DUMP_FIELD(QIndexDelta);
    DUMP_FIELD(LoopFilterLevelDelta);
    DUMP_FIELD(ReferenceFrame);
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtVP9TemporalLayers& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";

    for (mfxU16 i = 0; i < 8; i++)
    {
        str += dump(structName + ".Layer[" + ToString(i) + "]", _struct.Layer[i]) + "\n";
    }

    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxVP9TemporalLayer& _struct)
{
    std::string str;
    DUMP_FIELD(FrameRateScale);
    DUMP_FIELD(TargetKbps);
    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtVP9Param& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(FrameWidth);
    DUMP_FIELD(FrameHeight);
    DUMP_FIELD(WriteIVFHeaders);
    DUMP_FIELD_RESERVED(reserved1);
    DUMP_FIELD(QIndexDeltaLumaDC);
    DUMP_FIELD(QIndexDeltaChromaAC);
    DUMP_FIELD(QIndexDeltaChromaDC);
    DUMP_FIELD(NumTileRows);
    DUMP_FIELD(NumTileColumns);
    DUMP_FIELD_RESERVED(reserved);
    return str;
}

#if defined(ONEVPL_EXPERIMENTAL)
std::string DumpContext::dump(const std::string structName, const  mfxExtQualityInfoMode& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(QualityInfoMode);
    DUMP_FIELD_RESERVED(reserved);

    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtQualityInfoOutput& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(FrameOrder);
    str += structName + ".MSE[]=" + DUMP_RESERVED_ARRAY(_struct.MSE) + "\n";
    DUMP_FIELD_RESERVED(reserved1);
    DUMP_FIELD_RESERVED(reserved2);

    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtAV1ScreenContentTools& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(Palette);
    DUMP_FIELD(IntraBlockCopy);
    DUMP_FIELD_RESERVED(reserved);

    return str;
}

std::string DumpContext::dump(const std::string structName, const  mfxExtAlphaChannelEncCtrl& ExtAlphaCtrl)
{
    std::string str;
    str += dump(structName + ".Header", ExtAlphaCtrl.Header) + "\n";
    str += structName + ".EnableAlphaChannelEncoding=" + ToString(ExtAlphaCtrl.EnableAlphaChannelEncoding) + "\n";
    str += structName + ".AlphaChannelMode=" + ToString(ExtAlphaCtrl.AlphaChannelMode) + "\n";
    str += structName + ".AlphaChannelBitrateRatio=" + ToString(ExtAlphaCtrl.AlphaChannelBitrateRatio) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(ExtAlphaCtrl.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtAIEncCtrl& _struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(SaliencyEncoder);
    DUMP_FIELD_RESERVED(reserved);

    return str;
}
#endif