// Copyright (c) 2024 Intel Corporation
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
#include "hevcehw_xe2_lin.h"
#include "hevcehw_base_rext_lin.h"
#include "hevcehw_base_caps_lin.h"
#include "hevcehw_base_scc_lin.h"
#include "hevcehw_base_qp_modulation_lin.h"
#include "hevcehw_base_sao.h"
#include "hevcehw_base_scc.h"
#include "hevcehw_base_legacy.h"
#include "hevcehw_base_iddi_packer.h"
#include "hevcehw_base_iddi.h"
#include "hevcehw_base_parser.h"
#include "hevcehw_base_recon_info_lin.h"
#include "hevcehw_base_extddi.h"
#include "hevcehw_base_recon422.h"

namespace HEVCEHW
{
namespace Linux
{
namespace Xe2
{
using namespace HEVCEHW::Base;
using namespace HEVCEHW::Linux::Base;

MFXVideoENCODEH265_HW::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseGen(core, status, mode)
{
    TFeatureList newFeatures;

    newFeatures.emplace_back(new RExt(FEATURE_REXT));
    newFeatures.emplace_back(new SCC(FEATURE_SCC));
    newFeatures.emplace_back(new Caps(FEATURE_CAPS));
    newFeatures.emplace_back(new SAO(FEATURE_SAO));
    newFeatures.emplace_back(new QpModulation(FEATURE_QP_MODULATION));
    newFeatures.emplace_back(new ExtDDI(FEATURE_EXTDDI));
    newFeatures.emplace_back(new HEVCEHW::Base::Recon422(FEATURE_RECON422));

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
        auto& qnc = BQ<BQ_Query1NoCaps>::Get(*this);

        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_CAPS, Caps::BLK_SetDefaultsCallChain });

        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_SCC, SCC::BLK_SetLowPowerDefault });

        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_PARSER, HEVCEHW::Base::Parser::BLK_LoadSPSPPS }
            , { FEATURE_SCC, SCC::BLK_LoadSPSPPS });

        auto& qwc = BQ<BQ_Query1WithCaps>::Get(*this);
        Reorder(
            qwc
            , { HEVCEHW::Base::FEATURE_DDI_PACKER, HEVCEHW::Base::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_REXT, RExt::BLK_HardcodeCaps });

        Reorder(
            qwc
            , { HEVCEHW::Base::FEATURE_DDI_PACKER, HEVCEHW::Base::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_CAPS, Caps::BLK_HardcodeCaps });

        Reorder(
            qwc
            , { FEATURE_REXT, RExt::BLK_HardcodeCaps }
            , { HEVCEHW::Base::FEATURE_DDI_PACKER, HEVCEHW::Base::IDDIPacker::BLK_HardcodeCaps });

        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_DDI, HEVCEHW::Base::IDDI::BLK_SetDDIID }
            , { FEATURE_RECON422, HEVCEHW::Base::Recon422::BLK_SetRecon422Caps });
    }

    if (mode & INIT)
    {
        auto& iint = BQ<BQ_InitInternal>::Get(*this);
        Reorder(
            iint
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetSPS }
            , { FEATURE_SCC, SCC::BLK_SetSPSExt });
        Reorder(
            iint
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetPPS }
            , { FEATURE_SCC, SCC::BLK_SetPPSExt });
        Reorder(
            iint
            , { HEVCEHW::Base::FEATURE_RECON_INFO, HEVCEHW::Base::ReconInfo::BLK_SetRecInfo }
            , { FEATURE_REXT, RExt::BLK_SetRecInfo }
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
        , { FEATURE_SCC, SCC::BLK_PatchDDITask });

    return MFX_ERR_NONE;
}

} //namespace Linux
} //namespace Xe2
} //namespace HEVCEHW

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
