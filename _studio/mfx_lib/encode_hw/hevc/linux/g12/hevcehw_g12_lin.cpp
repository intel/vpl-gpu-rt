// Copyright (c) 2019-2021 Intel Corporation
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

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_g12_lin.h"
#include "hevcehw_base_rext_lin.h"
#include "hevcehw_g12_caps_lin.h"
#include "hevcehw_base_scc_lin.h"
#include "hevcehw_base_qp_modulation_lin.h"
#include "hevcehw_base_sao.h"
#include "hevcehw_base_scc.h"
#include "hevcehw_base_rext.h"
#include "hevcehw_base_legacy.h"
#include "hevcehw_base_iddi_packer.h"
#include "hevcehw_base_iddi.h"
#include "hevcehw_base_parser.h"
#include "hevcehw_base_recon_info_lin.h"

namespace HEVCEHW
{
namespace Linux
{
namespace Gen12
{
using namespace HEVCEHW::Gen12;

MFXVideoENCODEH265_HW::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseGen(core, status, mode)
{
    TFeatureList newFeatures;

    newFeatures.emplace_back(new HEVCEHW::Linux::Base::RExt(HEVCEHW::Base::FEATURE_REXT));
    newFeatures.emplace_back(new HEVCEHW::Linux::Base::SCC(HEVCEHW::Base::FEATURE_SCC));
    newFeatures.emplace_back(new HEVCEHW::Linux::Gen12::Caps(HEVCEHW::Linux::Gen12::FEATURE_CAPS));
    newFeatures.emplace_back(new HEVCEHW::Base::SAO(HEVCEHW::Base::FEATURE_SAO));
    newFeatures.emplace_back(new HEVCEHW::Linux::Base::QpModulation(HEVCEHW::Base::FEATURE_QP_MODULATION));

    InternalInitFeatures(status, mode, newFeatures);
}

void MFXVideoENCODEH265_HW::InternalInitFeatures(
    mfxStatus& status
    , eFeatureMode mode
    , TFeatureList& newFeatures)
{
    status = MFX_ERR_UNKNOWN;

    for (auto& pFeature : newFeatures)
        pFeature->Init(mode, *this);

    TBaseGen::m_features.splice(TBaseGen::m_features.end(), newFeatures);

    if (mode & (QUERY1 | QUERY_IO_SURF | INIT))
    {
        auto& qnc = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(*this);

        FeatureBlocks::Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_CAPS, Caps::BLK_SetDefaultsCallChain });
        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetLowPowerDefault }
            , { HEVCEHW::Base::FEATURE_SCC, HEVCEHW::Linux::Base::SCC::BLK_SetLowPowerDefault });
        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_PARSER, HEVCEHW::Base::Parser::BLK_LoadSPSPPS }
            , { HEVCEHW::Base::FEATURE_SCC, HEVCEHW::Linux::Base::SCC::BLK_LoadSPSPPS });

        auto& qwc = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(*this);
        FeatureBlocks::Reorder(
            qwc
            , { HEVCEHW::Base::FEATURE_DDI_PACKER, HEVCEHW::Base::IDDIPacker::BLK_HardcodeCaps }
            , { HEVCEHW::Base::FEATURE_REXT, HEVCEHW::Linux::Base::RExt::BLK_HardcodeCaps });
        FeatureBlocks::Reorder(
            qwc
            , { HEVCEHW::Base::FEATURE_DDI_PACKER, HEVCEHW::Base::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_CAPS, Caps::BLK_HardcodeCaps });

        FeatureBlocks::Reorder(
            qwc
            , { HEVCEHW::Base::FEATURE_REXT, HEVCEHW::Linux::Base::RExt::BLK_HardcodeCaps }
            , { HEVCEHW::Base::FEATURE_DDI_PACKER, HEVCEHW::Base::IDDIPacker::BLK_HardcodeCaps });
    }

    if (mode & INIT)
    {
        auto& iint = BQ<BQ_InitInternal>::Get(*this);
        Reorder(
            iint
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetSPS }
            , { HEVCEHW::Base::FEATURE_SCC, HEVCEHW::Linux::Base::SCC::BLK_SetSPSExt });
        Reorder(
            iint
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetPPS }
            , { HEVCEHW::Base::FEATURE_SCC, HEVCEHW::Linux::Base::SCC::SCC::BLK_SetPPSExt });
        Reorder(
            iint
            , { HEVCEHW::Base::FEATURE_RECON_INFO, HEVCEHW::Base::ReconInfo::BLK_SetRecInfo }
            , { HEVCEHW::Base::FEATURE_REXT, HEVCEHW::Linux::Base::RExt::BLK_SetRecInfo }
            , PLACE_AFTER);
    }

    status = MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265_HW::Init(mfxVideoParam *par)
{
    auto sts = TBaseGen::Init(par);
    MFX_CHECK_STS(sts);

    auto& st = BQ<BQ_SubmitTask>::Get(*this);
    Reorder(
        st
        , { HEVCEHW::Base::FEATURE_DDI, HEVCEHW::Base::IDDI::BLK_SubmitTask }
        , { HEVCEHW::Base::FEATURE_SCC, HEVCEHW::Base::SCC::BLK_PatchDDITask });

    return MFX_ERR_NONE;
}

} //namespace Linux
} //namespace Gen12
} //namespace HEVCEHW

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
