// Copyright (c) 2022-2024 Intel Corporation
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

#include "umc_defs.h"
#ifdef MFX_ENABLE_VVC_VIDEO_DECODE

#ifndef __UMC_VVC_DECODER_H
#define __UMC_VVC_DECODER_H

#include <mutex>
#include <memory>
#include <vector>
#include <deque>

#include "umc_media_data_ex.h"
#include "umc_video_decoder.h"
#include "umc_vvc_frame.h"
#include "umc_vvc_slice_decoding.h"
#include "umc_vvc_au_splitter.h"
#include "umc_vvc_headers_manager.h"

namespace UMC
{ class FrameAllocator; }

namespace UMC_VVC_DECODER
{
     class SEI_Payload_Storage;

/*********************************************************************/
// Skipping_VVC class routine
/*********************************************************************/
    class Skipping_VVC
    {
    public:
        virtual ~Skipping_VVC()
        {}

        // Check if frame should be skipped to decrease decoding delays
        bool IsShouldSkipFrame(VVCDecoderFrame *pFrame);
        void ChangeVideoDecodingSpeed(int32_t &num);
        void Reset();
        //...
    };

    //refine this class based on VVC Spec
    class SEI_Storer_VVC
    {
    public:

        struct SEI_Message
        {
            size_t             msg_size  = 0;
            size_t             offset    = 0;
            double             timestamp = 0.0;
            int32_t            isUsed    = 0;
            int32_t            nal_type  = 0;
            SEI_TYPE           type;
            uint8_t            *data     = nullptr;
            VVCDecoderFrame    *frame    = nullptr;
        };

        SEI_Storer_VVC();
        ~SEI_Storer_VVC();

        void Init();
        void Close();
        void Reset();

        void SetTimestamp(VVCDecoderFrame *frame);
        SEI_Message *AddMessage(UMC::MediaDataEx &nalUnit, int32_t auIndex);
        const SEI_Message *GetPayloadMessage();
        void SetFrame(VVCDecoderFrame *frame, int32_t auIndex);

    private:

        std::vector<uint8_t>        m_data;
        std::vector<SEI_Message>    m_payloads;
        size_t                      m_offset;
        int32_t                     m_lastUsed;
    };

    class VVCDecoderParams
        : public UMC::VideoDecoderParams
    {
        DYNAMIC_CAST_DECL(VVCDecoderParams, UMC::VideoDecoderParams)

    public:

        UMC::FrameAllocator  *allocator  = nullptr;
        uint32_t             async_depth = 0;
        uint32_t             io_pattern;
    };

    class VVCDecoder
        : public UMC::VideoDecoder, 
        public Skipping_VVC
    {
        friend class VideoDECODEVVC;

    public:

        VVCDecoder();
        ~VVCDecoder();

        static UMC::Status DecodeHeader(mfxBitstream *bs, mfxVideoParam *par);

        /* UMC::BaseCodec interface */
        UMC::Status Init(UMC::BaseCodecParams *) override;
        UMC::Status Close(void) override;
        UMC::Status GetFrame(UMC::MediaData *, UMC::MediaData *) override
        { return UMC::UMC_ERR_NOT_IMPLEMENTED; }
        UMC::Status Reset() override;
        UMC::Status GetInfo(UMC::BaseCodecParams *) override;

        void ThreadAwake();
        void InitAUs();
        /* UMC::VideoDecoder interface */
        UMC::Status ResetSkipCount() override
        { return UMC::UMC_ERR_NOT_IMPLEMENTED; }
        UMC::Status SkipVideoFrame(int32_t) override
        { return UMC::UMC_ERR_NOT_IMPLEMENTED; }
        uint32_t GetNumOfSkippedFrames() override
        { return 0; }

    public:

        // Find NAL units in new bitstream buffer and process them
        virtual UMC::Status AddOneFrame(UMC::MediaData *source);

        // Add a new bitstream data buffer to decoding
        virtual UMC::Status AddSource(UMC::MediaData *source);

        void countActiveRefs(uint32_t& NumShortTerm, uint32_t& NumLongTerm);

        bool IsEnoughForStartDecoding(bool);

        void PreventDPBFullness();

        bool IsFrameCompleted(VVCDecoderFrame* pFrame) const;

        bool isRandomAccessSkipPicture(VVCSlice *slc, bool mixedNaluInPicFlag, bool phOutputFlag);

        // Decode slice header start, set slice links to seq and picture
        virtual VVCSlice *DecodeSliceHeader(UMC::MediaData *);

        // Add a new slice to picture
        virtual UMC::Status AddSlice(VVCSlice *);

        // Check whether this slice should be skipped because of random access conditions. VVC spec ...
        bool IsSkipForCRA(const VVCSlice *pSlice);

        // Calculate NoRaslOutputFlag flag for specified slice
        void CheckCRA(const VVCSlice *pSlice);

        // Decode a bitstream header
        virtual UMC::Status DecodeHeaders(UMC::MediaDataEx *data);

        virtual VVCDecoderFrame *FindFrameByMemID(UMC::FrameMemID);

        virtual VVCDecoderFrame* FindOldestDisplayable(DPBType dpb);

        bool PrepareFrame(VVCDecoderFrame* pFrame);

        virtual UMC::Status RunDecoding();
        
        virtual void CalculateInfoForDisplay(DPBType m_dpb, uint32_t& countDisplayable, uint32_t& countDPBFullness, int32_t& maxUID);

        // Find a next frame ready to be output from decoder
        virtual VVCDecoderFrame *GetFrameToDisplay(bool force);

        // Check for frame completeness and get decoding errors
        //virtual bool QueryFrames(VVCDecoderFrame &) = 0;
        virtual bool QueryFrames() = 0;

        // Initialize mfxVideoParam structure based on decoded bitstream header values
        virtual UMC::Status FillVideoParam(mfxVideoParam *par/*, bool full*/);

        // Return raw sequence header
        virtual RawHeader_VVC *GetSeqHeader() { return &m_rawHeaders; }

        // Return a number of cached frames
        virtual uint8_t GetNumCachedFrames() const;

        // Set frame display time
        virtual void PostProcessDisplayFrame(VVCDecoderFrame *);

        // Set MFX video params
        virtual void SetVideoParams(const mfxVideoParam &);

        SEI_Storer_VVC *GetPayloadStorage() const { return m_seiMessages.get();}

        UMC::FrameMemID  GetRepeatedFrame() { return repeateFrame; }

        bool GetTOlsIdxExternalFlag()                  const { return m_tOlsIdxTidExternalSet; }
        void SetTOlsIdxExternalFlag(bool tOlsIdxExternalSet) { m_tOlsIdxTidExternalSet = tOlsIdxExternalSet; }
        bool GetTOlsIdxOpiFlag()                       const { return m_tOlsIdxTidOpiSet; }
        void SetTOlsIdxOpiFlag(bool tOlsIdxOpiSet)           { m_tOlsIdxTidOpiSet = tOlsIdxOpiSet; }

    protected:
        // Initialize frame's counter and corresponding params
        void InitFrameCounter(VVCDecoderFrame* pFrame, const VVCSlice* pSlice);

        void IncreaseRefPicListResetCount(DPBType dpb, VVCDecoderFrame* ExcludeFrame) const;

        // Action on new picture
        virtual bool IsNewPicture(); // returns true on full frame
        virtual void SetDPBSize(const VVCSeqParamSet *pSps/*, const VVCVideoParamSet* pVps, uint32_t& level_idc*/);
        void    DPBUpdate(VVCSlice* slice);
        VVCDecoderFrame* CreateUnavailablePicture(VVCSlice* pSlice, int rplx, int refPicIndex, const int iUnavailablePoc);
        void DetectUnavailableRefFrame(VVCSlice* slice);
        VVCDecoderFrame* GetOldestDisposable();
        virtual VVCDecoderFrame *GetFreeFrame();
        virtual VVCDecoderFrame *GetFrameBuffer(VVCSlice *);
        virtual void EliminateSliceErrors(VVCDecoderFrame *pFrame);
        virtual void OnFullFrame(VVCDecoderFrame* pFrame);
        virtual bool AddFrameToDecoding(VVCDecoderFrame* frame);
        virtual UMC::Status CompletePicture(VVCDecoderFrame *pFrame);
        UMC::Status UpdateDPB(const VVCSlice &slice);
        virtual void AllocateFrameData(UMC::VideoDataInfo *pInfo, UMC::FrameMemID, VVCDecoderFrame *pFrame) = 0;
        // If a frame has all slices found, add it to asynchronous decode queue
        UMC::Status CompleteDecodedFrames(VVCDecoderFrame** decoded);
        UMC::Status InitFreeFrame(VVCDecoderFrame* pFrame, VVCSlice* pSlice);
        virtual UMC::Status Submit(VVCDecoderFrame *pFrame) = 0; // Submit picture to driver
        VVCDecoderFrameInfo* FindAU();
        virtual void CompleteFrame(VVCDecoderFrame* frame);
        void RemoveAU(VVCDecoderFrameInfo* toRemove);
        virtual void SwitchCurrentAU();
        virtual bool GetNextTaskInternal() { return false; }

        uint32_t CalculateDPBSize(uint32_t profile_idc, uint32_t &level_idc, int32_t width, int32_t height, uint32_t num_ref_frames);

        std::mutex                      m_guard;
        UMC::FrameAllocator             *m_allocator;
        uint32_t                        m_counter;
        VVCDecoderParams                m_params;
        mfxVideoParam                   m_firstMfxVideoParams;
        DPBType                         m_dpb;        // storage of decoded frames
        VVCDecoderFrame                 *m_currFrame;
        int32_t                         m_frameOrder;
        uint32_t                        m_dpbSize;
        uint32_t                        m_DPBSizeEx;
        uint32_t                        m_sps_max_dec_pic_buffering;
        uint32_t                        m_sps_max_num_reorder_pics;
        double                          m_localDeltaFrameTime;
        bool                            m_useExternalFramerate;
        bool                            m_decOrder;
        double                          m_localFrameTime;
        int32_t                         m_maxUIDWhenDisplayed;
        std::unique_ptr<VVCSlice>       m_lastSlice;    // slice which could't be processed
        std::unique_ptr<SEI_Storer_VVC> m_seiMessages;
        PocDecoding                     m_pocDecoding;
        uint32_t                        m_level_idc;

        Heap_Objects                    m_ObjHeap;
        ParameterSetManager             m_currHeaders; // current active stream headers
        RawHeader_VVC                   m_rawHeaders;  // raw binary headers
        std::unique_ptr<Splitter_VVC>   m_splitter;    // au splitter
        UMC::VideoAccelerator           *m_va;

        int32_t                         m_RA_POC;
        uint8_t                         m_noRaslOutputFlag;
        NalUnitType                     m_IRAPType;
        bool                            m_checkCRAInsideResetProcess;

        typedef std::list<VVCDecoderFrame*> FrameQueue;
        FrameQueue                      m_decodingQueue;
        FrameQueue                      m_completedQueue;
        VVCDecoderFrameInfo             *m_FirstAU;
        bool                            m_IsShouldQuit;
        UMC::FrameMemID                 repeateFrame;//frame to be repeated

        bool                            m_lastNoOutputBeforeRecoveryFlag[VVC_MAX_VPS_LAYERS];
        bool                            m_firstSliceInSequence[VVC_MAX_VPS_LAYERS];
        bool                            m_prevSliceSkipped;
        int32_t                         m_skippedPOC;
        int32_t                         m_skippedLayerID;
        int32_t                         m_pocRandomAccess;
        int32_t                         m_prevGDRInSameLayerPOC[VVC_MAX_VPS_LAYERS] = {-VVC_MAX_INT}; // POC of the latest GDR picture
        int32_t                         m_prevGDRInSameLayerRecoveryPOC[VVC_MAX_VPS_LAYERS] = {-VVC_MAX_INT}; // Recovery POC number of the latest GDR picture
        bool                            m_prevPicSkipped = true;
        bool                            m_gdrRecoveryPeriod[VVC_MAX_VPS_LAYERS] = { false };
        bool                            m_tOlsIdxTidExternalSet;
        bool                            m_tOlsIdxTidOpiSet;

    private:

        // Find a reusable frame and initialize it with slice parameters
        VVCDecoderFrame* StartFrame(VVCSlice *slice);
        // Decode SEI NAL unit
        UMC::Status DecodeSEI(UMC::MediaDataEx *nalUnit);

        // Decode video parameters set NAL unit
        UMC::Status xDecodeVPS(VVCHeadersBitstream *);
        // Decode sequence parameters set NAL unit
        UMC::Status xDecodeSPS(VVCHeadersBitstream *);
        // Decode picture parameters set NAL unit
        UMC::Status xDecodePPS(VVCHeadersBitstream *);
        // Decode picture header NAL unit
        UMC::Status xDecodePH(VVCHeadersBitstream *);
        // Decode adaption parameters set NAL unit
        UMC::Status xDecodeAPS(VVCHeadersBitstream*);
        UMC::Status xDecodeOPI(VVCHeadersBitstream*);
    };
}

#endif // __UMC_VVC_DECODER_H
#endif // MFX_ENABLE_VVC_VIDEO_DECODE

