// Copyright (c) 2019-2022 Intel Corporation
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


#include "hevcehw_xe_hpm_lin.h"
#include "hevcehw_base_legacy.h"
#include "hevcehw_base_extddi.h"
#include "hevcehw_base_caps_lin.h"
#include "hevcehw_g12_caps.h"
#include "hevcehw_base_recon422_ext.h"
#include "hevcehw_base_iddi.h"

namespace HEVCEHW
{
namespace Linux
{
namespace Xe_HPM
{
MFXVideoENCODEH265_HW::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseImpl(core, status, mode)
{
    TFeatureList newFeatures;

    newFeatures.emplace_back(new HEVCEHW::Linux::Base::Caps(HEVCEHW::Base::FEATURE_CAPS));
    newFeatures.emplace_back(new HEVCEHW::Base::ExtDDI(HEVCEHW::Base::FEATURE_EXTDDI));
    newFeatures.emplace_back(new HEVCEHW::Base::Recon422EXT(FEATURE_RECON422EXT));

    for (auto& pFeature : newFeatures)
        pFeature->Init(mode, *this);

    m_features.splice(m_features.end(), newFeatures);

    if (mode & (QUERY1 | QUERY_IO_SURF | INIT))
    {
        auto& qnc = BQ<BQ_Query1NoCaps>::Get(*this);

        qnc.splice(qnc.begin(), qnc, Get(qnc, { HEVCEHW::Base::FEATURE_CAPS, HEVCEHW::Linux::Base::Caps::BLK_SetLowPower }));

        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetLowPowerDefault }
            , { HEVCEHW::Base::FEATURE_CAPS, HEVCEHW::Linux::Base::Caps::BLK_SetDefaultsCallChain });

        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_RECON422EXT, HEVCEHW::Base::Recon422EXT::BLK_SetCallChain });//featureID, blockID

        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_DDI, HEVCEHW::Base::IDDI::BLK_SetDDIID }
            , { FEATURE_RECON422EXT, HEVCEHW::Base::Recon422EXT::BLK_SetRecon422Caps });

        auto& qwc = BQ<BQ_Query1WithCaps>::Get(*this);

        Reorder(
            qwc
            , { HEVCEHW::Gen12::FEATURE_CAPS, HEVCEHW::Gen12::Caps::BLK_HardcodeCaps }
            , { HEVCEHW::Base::FEATURE_CAPS, HEVCEHW::Linux::Base::Caps::BLK_HardcodeCaps }
        , PLACE_AFTER);
    }
}

}}} //namespace HEVCEHW::Linux::Xe_HPM


#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)