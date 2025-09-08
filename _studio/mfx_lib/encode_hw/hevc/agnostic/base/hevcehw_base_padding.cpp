// Copyright (c) 2025 Intel Corporation
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

#include "hevcehw_base_padding.h"

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

namespace HEVCEHW
{
    namespace Base
    {
        bool Padding::IsPaddingNeeded(StorageRW& global, StorageRW& local)
        {
            auto& par = Glob::VideoParam::Get(global);
            auto& fi = par.mfx.FrameInfo;
            PPS& pps = Glob::PPS::Get(global);
            const mfxExtHEVCParam& HEVCParam = ExtBuffer::Get(par);
            const mfxExtCodingOption& CO = ExtBuffer::Get(par);
            const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par);
            auto& rawInfo = Tmp::RawInfo::Get(local);
            auto& recInfo = Tmp::RecInfo::Get(local);
            bool bIsVC = (CO3.ScenarioInfo == MFX_SCENARIO_VIDEO_CONFERENCE);
            bool bIsNV12 = par.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12;
            mfxU16 alignedHeight = mfx::align2_value(HEVCParam.PicHeightInLumaSamples, 16u);
            bool bIsUnaligned = (HEVCParam.PicHeightInLumaSamples % 8 == 0) && (HEVCParam.PicHeightInLumaSamples % 16 != 0) && (rawInfo.Info.Height >= alignedHeight) && (recInfo.Info.Height >= alignedHeight);
            bool bIsMultiTile = (pps.tiles_enabled_flag != 0);
            bool isTCBRC = (IsOn(CO3.LowDelayBRC) && IsOff(CO.NalHrdConformance) &&
                (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR ||
                 par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
                 par.mfx.RateControlMethod == MFX_RATECONTROL_VCM));
            bool hasCrop = (HEVCParam.PicHeightInLumaSamples - fi.CropH - fi.CropY) > 0;
            return bIsNV12 && bIsVC && bIsUnaligned && !bIsMultiTile && !isTCBRC && !hasCrop;
        }

        void Padding::InsertFramePadding(StorageRW& global)
        {
            SPS& sps = Glob::SPS::Get(global);
            mfxU16 SubHeightC[4] = { 1,2,1,1 };
            mfxU16 cropUnitY = SubHeightC[sps.chroma_format_idc];
            sps.conf_win_bottom_offset = (mfx::align2_value(sps.pic_height_in_luma_samples, 16u) - sps.pic_height_in_luma_samples) / cropUnitY;
            sps.conformance_window_flag = 1;
            sps.pic_height_in_luma_samples = mfx::align2_value(sps.pic_height_in_luma_samples, 16u);
        }

        void Padding::ResetFramePadding(StorageRW& global)
        {
            auto& par = Glob::VideoParam::Get(global);
            const mfxExtHEVCParam& HEVCParam = ExtBuffer::Get(par);
            auto& fi = par.mfx.FrameInfo;
            SPS& sps = Glob::SPS::Get(global);
            sps.pic_height_in_luma_samples = HEVCParam.PicHeightInLumaSamples;
            mfxU16 SubHeightC[4] = { 1,2,1,1 };
            mfxU16 cropUnitY = SubHeightC[sps.chroma_format_idc];
            sps.conf_win_bottom_offset = (sps.pic_height_in_luma_samples - fi.CropH - fi.CropY) / cropUnitY;
            sps.conformance_window_flag = sps.conf_win_left_offset
                || sps.conf_win_right_offset
                || sps.conf_win_top_offset
                || sps.conf_win_bottom_offset;
        }

        void Padding::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
        {
            Push(BLK_ResetFramePadding
                , [this](
                    StorageRW& global
                    , StorageRW& local) -> mfxStatus
                {
                    MFX_CHECK(IsPaddingNeeded(global, local), MFX_ERR_NONE);
                    ResetFramePadding(global);
                    return MFX_ERR_NONE;
                });

            Push(BLK_InsertFramePadding
                , [this](
                    StorageRW& global
                    , StorageRW& local) -> mfxStatus
                {
                    MFX_CHECK(IsPaddingNeeded(global, local), MFX_ERR_NONE);
                    InsertFramePadding(global);
                    return MFX_ERR_NONE;
                });
        }

        void Padding::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
        {
            Push(BLK_ResetStateResetFramePadding
                , [this](
                    StorageRW& global
                    , StorageRW& local) -> mfxStatus
                {
                    MFX_CHECK(IsPaddingNeeded(global, local), MFX_ERR_NONE);
                    ResetFramePadding(global);
                    return MFX_ERR_NONE;
                });

            Push(BLK_ResetStateInsertFramePadding
                , [this](
                    StorageRW& global
                    , StorageRW& local) -> mfxStatus
                {
                    MFX_CHECK(IsPaddingNeeded(global, local), MFX_ERR_NONE);
                    InsertFramePadding(global);
                    return MFX_ERR_NONE;
                });
        }
    } //namespace Base
} //namespace HEVCEHW

#endif // defined(MFX_ENABLE_H265_VIDEO_ENCODE)