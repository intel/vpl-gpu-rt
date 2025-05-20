// Copyright (c) 2008-2024 Intel Corporation
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

STRUCT(mfxI16Pair,
    FIELD_T(mfxI16, x)
    FIELD_T(mfxI16, y)
)

STRUCT(mfxHDLPair,
    FIELD_T(mfxHDL, first )
    FIELD_T(mfxHDL, second)
)

STRUCT(mfxExtBuffer,
    FIELD_T(mfx4CC, BufferId)
    FIELD_T(mfxU32, BufferSz)
)

STRUCT(mfxVersion,
    FIELD_T(mfxU16, Minor  )
    FIELD_T(mfxU16, Major  )
    FIELD_T(mfxU32, Version)
)

STRUCT(mfxStructVersion,
    FIELD_T(mfxU8, Minor)
    FIELD_T(mfxU8, Major)
    FIELD_T(mfxU16, Version)
)

STRUCT(mfxBitstream,
    FIELD_T(mfxEncryptedData*, EncryptedData  )
    FIELD_T(mfxExtBuffer **  , ExtParam       )
    FIELD_T(mfxU16           , NumExtParam    )
    FIELD_T(mfxI64           , DecodeTimeStamp)
    FIELD_T(mfxU64           , TimeStamp      )
    FIELD_T(mfxU8*           , Data           )
    FIELD_T(mfxU32           , DataOffset     )
    FIELD_T(mfxU32           , DataLength     )
    FIELD_T(mfxU32           , MaxLength      )
    FIELD_T(mfxU16           , PicStruct      )
    FIELD_T(mfxU16           , FrameType      )
    FIELD_T(mfxU16           , DataFlag       )
)

STRUCT(mfxFrameId,
    FIELD_T(mfxU16, TemporalId  )
    FIELD_T(mfxU16, PriorityId  )
    FIELD_T(mfxU16, DependencyId)
    FIELD_T(mfxU16, QualityId   )
    FIELD_T(mfxU16, ViewId      )
)

STRUCT(mfxFrameInfo,
    FIELD_S(mfxFrameId, FrameId)
    FIELD_T(mfxU16, BitDepthLuma  )
    FIELD_T(mfxU16, BitDepthChroma)
    FIELD_T(mfxU16, Shift         )
    FIELD_T(mfx4CC, FourCC        )
    FIELD_T(mfxU16, Width         )
    FIELD_T(mfxU16, Height        )
    FIELD_T(mfxU16, CropX         )
    FIELD_T(mfxU16, CropY         )
    FIELD_T(mfxU16, CropW         )
    FIELD_T(mfxU16, CropH         )
    FIELD_T(mfxU32, FrameRateExtN )
    FIELD_T(mfxU32, FrameRateExtD )
    FIELD_T(mfxU16, AspectRatioW  )
    FIELD_T(mfxU16, AspectRatioH  )
    FIELD_T(mfxU16, PicStruct     )
    FIELD_T(mfxU16, ChromaFormat  )
)

STRUCT(mfxFrameData,
    FIELD_T(mfxExtBuffer**, ExtParam    )
    FIELD_T(mfxU16        , NumExtParam )
    FIELD_T(mfxU16  , PitchHigh  )
    FIELD_T(mfxU64  , TimeStamp  )
    FIELD_T(mfxU32  , FrameOrder )
    FIELD_T(mfxU16  , Locked     )
    FIELD_T(mfxU16  , Pitch      )
    FIELD_T(mfxU16  , PitchLow   )
    FIELD_T(mfxU8 * , Y          )
    FIELD_T(mfxU16* , Y16        )
    FIELD_T(mfxU8 * , R          )
    FIELD_T(mfxU8 * , UV         )
    FIELD_T(mfxU8 * , VU         )
    FIELD_T(mfxU8 * , CbCr       )
    FIELD_T(mfxU8 * , CrCb       )
    FIELD_T(mfxU8 * , Cb         )
    FIELD_T(mfxU8 * , U          )
    FIELD_T(mfxU16* , U16        )
    FIELD_T(mfxU8 * , G          )
    FIELD_T(mfxU8 * , Cr         )
    FIELD_T(mfxU8 * , V          )
    FIELD_T(mfxU16* , V16        )
    FIELD_T(mfxU8 * , B          )
    FIELD_T(mfxU8 * , A          )
    FIELD_T(mfxMemId,  MemId     )
    FIELD_T(mfxU16  , Corrupted  )
    FIELD_T(mfxU16  , DataFlag   )
)

STRUCT(mfxFrameSurface1,
    FIELD_S(mfxFrameInfo, Info)
    FIELD_S(mfxFrameData, Data)
)

STRUCT(mfxInfoMFX,
    FIELD_T(mfxU16, LowPower          )
    FIELD_T(mfxU16, BRCParamMultiplier)
    FIELD_S(mfxFrameInfo, FrameInfo   )
    FIELD_T(mfx4CC, CodecId           )
    FIELD_T(mfxU16, CodecProfile      )
    FIELD_T(mfxU16, CodecLevel        )
    FIELD_T(mfxU16, NumThread         )
    FIELD_T(mfxU16, TargetUsage       )
    FIELD_T(mfxU16, GopPicSize        )
    FIELD_T(mfxU16, GopRefDist        )
    FIELD_T(mfxU16, GopOptFlag        )
    FIELD_T(mfxU16, IdrInterval       )
    FIELD_T(mfxU16, RateControlMethod )
    FIELD_T(mfxU16, InitialDelayInKB  )
    FIELD_T(mfxU16, QPI               )
    FIELD_T(mfxU16, Accuracy          )
    FIELD_T(mfxU16, BufferSizeInKB    )
    FIELD_T(mfxU16, TargetKbps        )
    FIELD_T(mfxU16, QPP               )
    FIELD_T(mfxU16, ICQQuality        )
    FIELD_T(mfxU16, MaxKbps           )
    FIELD_T(mfxU16, QPB               )
    FIELD_T(mfxU16, Convergence       )
    FIELD_T(mfxU16, NumSlice          )
    FIELD_T(mfxU16, NumRefFrame       )
    FIELD_T(mfxU16, EncodedOrder      )
    FIELD_T(mfxU16, DecodedOrder      )
    FIELD_T(mfxU16, ExtendedPicStruct )
    FIELD_T(mfxU16, TimeStampCalc     )
    FIELD_T(mfxU16, SliceGroupsPresent)
    FIELD_T(mfxU16, MaxDecFrameBuffering )
    FIELD_T(mfxU16, EnableReallocRequest)
    FIELD_T(mfxU16, FilmGrain         )
    FIELD_T(mfxU16, JPEGChromaFormat  )
    FIELD_T(mfxU16, Rotation          )
    FIELD_T(mfxU16, JPEGColorFormat   )
    FIELD_T(mfxU16, InterleavedDec    )
    FIELD_T(mfxU16, Interleaved       )
    FIELD_T(mfxU16, Quality           )
    FIELD_T(mfxU16, RestartInterval   )
)

STRUCT(mfxInfoVPP,
    FIELD_S(mfxFrameInfo, In)
    FIELD_S(mfxFrameInfo, Out)
)

STRUCT(mfxVideoParam,
    FIELD_S(mfxInfoMFX, mfx)
    FIELD_S(mfxInfoVPP, vpp)
    FIELD_T(mfxU16        , AsyncDepth  )
    FIELD_T(mfxU16        , Protected   )
    FIELD_T(mfxU16        , IOPattern   )
    FIELD_T(mfxExtBuffer**, ExtParam    )
    FIELD_T(mfxU16        , NumExtParam )
)

STRUCT(mfxExtCodingOption,
    FIELD_S(mfxExtBuffer, Header               )
    FIELD_T(mfxU16      , reserved1            )
    FIELD_T(mfxU16      , RateDistortionOpt    )
    FIELD_T(mfxU16      , MECostType           )
    FIELD_T(mfxU16      , MESearchType         )
    FIELD_S(mfxI16Pair  , MVSearchWindow       )
    FIELD_T(mfxU16      , EndOfSequence        )
    FIELD_T(mfxU16      , FramePicture         )
    FIELD_T(mfxU16      , CAVLC                )
    FIELD_T(mfxU16      , RecoveryPointSEI     )
    FIELD_T(mfxU16      , ViewOutput           )
    FIELD_T(mfxU16      , NalHrdConformance    )
    FIELD_T(mfxU16      , SingleSeiNalUnit     )
    FIELD_T(mfxU16      , VuiVclHrdParameters  )
    FIELD_T(mfxU16      , RefPicListReordering )
    FIELD_T(mfxU16      , ResetRefList         )
    FIELD_T(mfxU16      , RefPicMarkRep        )
    FIELD_T(mfxU16      , FieldOutput          )
    FIELD_T(mfxU16      , IntraPredBlockSize   )
    FIELD_T(mfxU16      , InterPredBlockSize   )
    FIELD_T(mfxU16      , MVPrecision          )
    FIELD_T(mfxU16      , MaxDecFrameBuffering )
    FIELD_T(mfxU16      , AUDelimiter          )
    FIELD_T(mfxU16      , EndOfStream          )
    FIELD_T(mfxU16      , PicTimingSEI         )
    FIELD_T(mfxU16      , VuiNalHrdParameters  )
)

STRUCT(mfxExtCodingOption2,
    FIELD_S(mfxExtBuffer, Header          )
    FIELD_T(mfxU16      , IntRefType      )
    FIELD_T(mfxU16      , IntRefCycleSize )
    FIELD_T(mfxI16      , IntRefQPDelta   )
    FIELD_T(mfxU32      , MaxFrameSize    )
    FIELD_T(mfxU32      , MaxSliceSize    )
    FIELD_T(mfxU16      , BitrateLimit    )
    FIELD_T(mfxU16      , MBBRC           )
    FIELD_T(mfxU16      , ExtBRC          )
    FIELD_T(mfxU16      , LookAheadDepth  )
    FIELD_T(mfxU16      , Trellis         )
    FIELD_T(mfxU16      , RepeatPPS       )
    FIELD_T(mfxU16      , BRefType        )
    FIELD_T(mfxU16      , AdaptiveI       )
    FIELD_T(mfxU16      , AdaptiveB       )
    FIELD_T(mfxU16      , LookAheadDS     )
    FIELD_T(mfxU16      , NumMbPerSlice   )
    FIELD_T(mfxU16      , SkipFrame       )
    FIELD_T(mfxU8       , MinQPI          )
    FIELD_T(mfxU8       , MaxQPI          )
    FIELD_T(mfxU8       , MinQPP          )
    FIELD_T(mfxU8       , MaxQPP          )
    FIELD_T(mfxU8       , MinQPB          )
    FIELD_T(mfxU8       , MaxQPB          )
    FIELD_T(mfxU16      , FixedFrameRate       )
    FIELD_T(mfxU16      , DisableDeblockingIdc )
    FIELD_T(mfxU16      , DisableVUI        )
    FIELD_T(mfxU16      , BufferingPeriodSEI)
    FIELD_T(mfxU16      , EnableMAD         )
    FIELD_T(mfxU16      , UseRawRef         )
)

STRUCT(mfxExtVPPDoNotUse,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32 , NumAlg)
    FIELD_T(mfxU32*, AlgList)
)

STRUCT(mfxExtVPPDenoise,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, DenoiseFactor)
)

STRUCT(mfxExtVPPDetail,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, DetailFactor)
)

STRUCT(mfxExtVPPProcAmp,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxF64, Brightness  )
    FIELD_T(mfxF64, Contrast    )
    FIELD_T(mfxF64, Hue         )
    FIELD_T(mfxF64, Saturation  )
)

STRUCT(mfxEncodeStat,
    FIELD_T(mfxU32, NumFrame            )
    FIELD_T(mfxU64, NumBit              )
    FIELD_T(mfxU32, NumCachedFrame      )
)

STRUCT(mfxDecodeStat,
    FIELD_T(mfxU32, NumFrame        )
    FIELD_T(mfxU32, NumSkippedFrame )
    FIELD_T(mfxU32, NumError        )
    FIELD_T(mfxU32, NumCachedFrame  )
)

STRUCT(mfxVPPStat,
    FIELD_T(mfxU32, NumFrame        )
    FIELD_T(mfxU32, NumCachedFrame  )
)

STRUCT(mfxExtVppAuxData,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, PicStruct       )
    FIELD_T(mfxU16, SceneChangeRate )
    FIELD_T(mfxU16, RepeatedFrame   )
)

STRUCT(mfxPayload,
    FIELD_T(mfxU8*, Data    )
    FIELD_T(mfxU32, NumBit  )
    FIELD_T(mfxU16, Type    )
    FIELD_T(mfxU16, BufSize )
)

STRUCT(mfxEncodeCtrl,
    FIELD_S(mfxExtBuffer  , Header     )
    FIELD_T(mfxU16        , MfxNalUnitType)
    FIELD_T(mfxU16        , SkipFrame  )
    FIELD_T(mfxU16        , QP         )
    FIELD_T(mfxU16        , FrameType  )
    FIELD_T(mfxU16        , NumExtParam)
    FIELD_T(mfxU16        , NumPayload )
    FIELD_T(mfxExtBuffer**, ExtParam   )
    FIELD_T(mfxPayload**  , Payload    )
)

STRUCT(mfxFrameAllocRequest,
    FIELD_S(mfxFrameInfo, Info)
    FIELD_T(mfxU16, Type              )
    FIELD_T(mfxU16, NumFrameMin       )
    FIELD_T(mfxU16, NumFrameSuggested )
    )

STRUCT(mfxFrameAllocResponse,
    FIELD_T(mfxMemId*, mids           )
    FIELD_T(mfxU16   , NumFrameActual )
)

STRUCT(mfxExtCodingOptionSPSPPS,
    FIELD_S(mfxExtBuffer, Header    )
    FIELD_T(mfxU8*      , SPSBuffer )
    FIELD_T(mfxU8*      , PPSBuffer )
    FIELD_T(mfxU16      , SPSBufSize)
    FIELD_T(mfxU16      , PPSBufSize)
    FIELD_T(mfxU16      , SPSId     )
    FIELD_T(mfxU16      , PPSId     )
)

STRUCT(mfxExtVideoSignalInfo,
    FIELD_S(mfxExtBuffer, Header    )
    FIELD_T(mfxU16, VideoFormat             )
    FIELD_T(mfxU16, VideoFullRange          )
    FIELD_T(mfxU16, ColourDescriptionPresent)
    FIELD_T(mfxU16, ColourPrimaries         )
    FIELD_T(mfxU16, TransferCharacteristics )
    FIELD_T(mfxU16, MatrixCoefficients      )
)

STRUCT(mfxExtVPPDoUse,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32 , NumAlg)
    FIELD_T(mfxU32*, AlgList)
)

STRUCT(mfxExtOpaqueSurfaceAlloc_InOut,
    FIELD_T(mfxFrameSurface1 ** , Surfaces  )
    FIELD_T(mfxU16              , Type      )
    FIELD_T(mfxU16              , NumSurface)
)

STRUCT(mfxExtOpaqueSurfaceAlloc,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_S(mfxExtOpaqueSurfaceAlloc_InOut, In)
    FIELD_S(mfxExtOpaqueSurfaceAlloc_InOut, Out)
)

STRUCT(mfxExtAVCRefListCtrl_Entry,
    FIELD_T(mfxU32, FrameOrder  )
    FIELD_T(mfxU16, PicStruct   )
    FIELD_T(mfxU16, ViewId      )
    FIELD_T(mfxU16, LongTermIdx )
)

STRUCT(mfxExtAVCRefListCtrl,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, NumRefIdxL0Active)
    FIELD_T(mfxU16, NumRefIdxL1Active)
    FIELD_S(mfxExtAVCRefListCtrl_Entry, PreferredRefList)
    FIELD_S(mfxExtAVCRefListCtrl_Entry, RejectedRefList )
    FIELD_S(mfxExtAVCRefListCtrl_Entry, LongTermRefList )
    FIELD_T(mfxU16, ApplyLongTermIdx )
)

STRUCT(mfxExtVPPFrameRateConversion,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, Algorithm)
)

STRUCT(mfxExtVPPImageStab,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, Mode)
)

STRUCT(mfxExtPictureTimingSEI_TimeStamp,
    FIELD_T(mfxU16, ClockTimestampFlag)
    FIELD_T(mfxU16, CtType            )
    FIELD_T(mfxU16, NuitFieldBasedFlag)
    FIELD_T(mfxU16, CountingType      )
    FIELD_T(mfxU16, FullTimestampFlag )
    FIELD_T(mfxU16, DiscontinuityFlag )
    FIELD_T(mfxU16, CntDroppedFlag    )
    FIELD_T(mfxU16, NFrames           )
    FIELD_T(mfxU16, SecondsFlag       )
    FIELD_T(mfxU16, MinutesFlag       )
    FIELD_T(mfxU16, HoursFlag         )
    FIELD_T(mfxU16, SecondsValue      )
    FIELD_T(mfxU16, MinutesValue      )
    FIELD_T(mfxU16, HoursValue        )
    FIELD_T(mfxU32, TimeOffset        )
)

STRUCT(mfxExtPictureTimingSEI,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_S(mfxExtPictureTimingSEI_TimeStamp, TimeStamp)
)

STRUCT(mfxExtAvcTemporalLayers_Layer,
    FIELD_T(mfxU16, Scale)
)

STRUCT(mfxExtAvcTemporalLayers,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, BaseLayerPID)
    FIELD_S(mfxExtAvcTemporalLayers_Layer, Layer)
)

STRUCT(mfxTemporalLayer,
    FIELD_T(mfxU16, FrameRateScale)
    FIELD_T(mfxU32, InitialDelayInKB)
    FIELD_T(mfxU32, BufferSizeInKB)
    FIELD_T(mfxU32, TargetKbps)
    FIELD_T(mfxU32, MaxKbps)
)

STRUCT(mfxExtTemporalLayers,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, NumLayers)
    FIELD_T(mfxU16, BaseLayerPID)
    FIELD_T(mfxTemporalLayer*, Layers)
)

STRUCT(mfxExtEncoderCapability,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32, MBPerSec)
)

STRUCT(mfxExtEncoderResetOption,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, StartNewSequence)
)

STRUCT(mfxExtAVCEncodedFrameInfo_RefList,
    FIELD_T(mfxU32, FrameOrder  )
    FIELD_T(mfxU16, PicStruct   )
    FIELD_T(mfxU16, LongTermIdx )
)

STRUCT(mfxExtAVCEncodedFrameInfo,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32, FrameOrder  )
    FIELD_T(mfxU16, PicStruct   )
    FIELD_T(mfxU16, LongTermIdx )
    FIELD_T(mfxU32, MAD         )
    FIELD_T(mfxU16, BRCPanicMode)
    FIELD_T(mfxU16, QP          )
    FIELD_T(mfxU32, SecondFieldOffset  )
    FIELD_S(mfxExtAVCEncodedFrameInfo_RefList, UsedRefListL0)
    FIELD_S(mfxExtAVCEncodedFrameInfo_RefList, UsedRefListL1)
)

STRUCT(mfxVPPCompInputStream,
    FIELD_T(mfxU32, DstX             )
    FIELD_T(mfxU32, DstY             )
    FIELD_T(mfxU32, DstW             )
    FIELD_T(mfxU32, DstH             )
    FIELD_T(mfxU16, LumaKeyEnable    )
    FIELD_T(mfxU16, LumaKeyMin       )
    FIELD_T(mfxU16, LumaKeyMax       )
    FIELD_T(mfxU16, GlobalAlphaEnable)
    FIELD_T(mfxU16, GlobalAlpha      )
    FIELD_T(mfxU16, PixelAlphaEnable )
    FIELD_T(mfxU16, TileId )
)

STRUCT(mfxExtVPPComposite,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, Y)
    FIELD_T(mfxU16, U)
    FIELD_T(mfxU16, V)
    FIELD_T(mfxU16, R)
    FIELD_T(mfxU16, G)
    FIELD_T(mfxU16, B)
    FIELD_T(mfxU16, NumTiles)
    FIELD_T(mfxU16, NumInputStream)
    FIELD_T(mfxVPPCompInputStream*, InputStream)
)

STRUCT(mfxExtColorConversion,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, ChromaSiting)
)

STRUCT(mfxExtVPPVideoSignalInfo_InOut,
    FIELD_T(mfxU16, TransferMatrix )
    FIELD_T(mfxU16, NominalRange   )
)

STRUCT(mfxExtVPPVideoSignalInfo,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_S(mfxExtVPPVideoSignalInfo_InOut, In )
    FIELD_S(mfxExtVPPVideoSignalInfo_InOut, Out)
)

STRUCT(mfxExtEncoderROI_Entry,
    FIELD_T(mfxU32, Left    )
    FIELD_T(mfxU32, Top     )
    FIELD_T(mfxU32, Right   )
    FIELD_T(mfxU32, Bottom  )
    FIELD_T(mfxI16, Priority)
)

STRUCT(mfxExtEncoderROI,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxI16, NumROI)
    FIELD_T(mfxU16, ROIMode)
    FIELD_S(mfxExtEncoderROI_Entry, ROI)
)

STRUCT(mfxExtVPPDeinterlacing,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, Mode)
    FIELD_T(mfxU16, TelecinePattern)
    FIELD_T(mfxU16, TelecineLocation)
)

STRUCT(mfxExtVP8CodingOption,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, Version)
    FIELD_T(mfxU16, EnableMultipleSegments)
    FIELD_T(mfxU16, LoopFilterType)
)

STRUCT(mfxExtAVCRefLists_mfxRefPic,
    FIELD_T(mfxU32, FrameOrder)
    FIELD_T(mfxU16, PicStruct )
)

STRUCT(mfxExtAVCRefLists,
    FIELD_S(mfxExtBuffer, Header           )
    FIELD_T(mfxU16      , NumRefIdxL0Active)
    FIELD_T(mfxU16      , NumRefIdxL1Active)
    FIELD_S(mfxExtAVCRefLists_mfxRefPic, RefPicList0)
    FIELD_S(mfxExtAVCRefLists_mfxRefPic, RefPicList1)
)

STRUCT(mfxExtCodingOption3,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, NumSliceI)
    FIELD_T(mfxU16, NumSliceP)
    FIELD_T(mfxU16, NumSliceB)
    FIELD_T(mfxU16, WinBRCMaxAvgKbps)
    FIELD_T(mfxU16, WinBRCSize)
    FIELD_T(mfxU16, QVBRQuality)
    FIELD_T(mfxU16, EnableMBQP)
    FIELD_T(mfxU16, IntRefCycleDist)
    FIELD_T(mfxU16, DirectBiasAdjustment)
    FIELD_T(mfxU16, GlobalMotionBiasAdjustment)/* tri-state option */
    FIELD_T(mfxU16, MVCostScalingFactor)
    FIELD_T(mfxU16, MBDisableSkipMap)/* tri-state option */
    FIELD_T(mfxU16, WeightedPred)
    FIELD_T(mfxU16, WeightedBiPred)
    FIELD_T(mfxU16, AspectRatioInfoPresent) /* tri-state option */
    FIELD_T(mfxU16, OverscanInfoPresent) /* tri-state option */
    FIELD_T(mfxU16, OverscanAppropriate) /* tri-state option */
    FIELD_T(mfxU16, TimingInfoPresent) /* tri-state option */
    FIELD_T(mfxU16, BitstreamRestriction) /* tri-state option */
    FIELD_T(mfxU16, LowDelayHrd) /* tri-state option */
    FIELD_T(mfxU16, MotionVectorsOverPicBoundaries) /* tri-state option */
    FIELD_T(mfxU16, ScenarioInfo)
    FIELD_T(mfxU16, ContentInfo)
    FIELD_T(mfxU16, PRefType)
    FIELD_T(mfxU16, FadeDetection) /* tri-state option */
    FIELD_T(mfxU16, GPB)
    FIELD_T(mfxU32, MaxFrameSizeI)
    FIELD_T(mfxU32, MaxFrameSizeP)
    FIELD_T(mfxU16, EnableQPOffset)
    FIELD_T(mfxI16, QPOffset)
    FIELD_T(mfxU16, NumRefActiveP)
    FIELD_T(mfxU16, NumRefActiveBL0)
    FIELD_T(mfxU16, NumRefActiveBL1)
    FIELD_T(mfxU16, TransformSkip)
    FIELD_T(mfxU16, TargetChromaFormatPlus1)
    FIELD_T(mfxU16, TargetBitDepthLuma)
    FIELD_T(mfxU16, TargetBitDepthChroma)
    FIELD_T(mfxU16, BRCPanicMode)
    FIELD_T(mfxU16, LowDelayBRC)
    FIELD_T(mfxU16, EnableMBForceIntra)
    FIELD_T(mfxU16, AdaptiveMaxFrameSize)
    FIELD_T(mfxU16, RepartitionCheckEnable) /* tri-state option */
    FIELD_T(mfxU16, EncodedUnitsInfo)
    FIELD_T(mfxU16, EnableNalUnitType)
    FIELD_T(mfxU16, ExtBrcAdaptiveLTR)
)

STRUCT(mfxQPandMode,
    FIELD_T(mfxU8,  QP)
    FIELD_T(mfxU16, Mode)
)

STRUCT(mfxExtMBQP,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32      , NumQPAlloc)
    FIELD_T(mfxU8*      , QP)
)

STRUCT(mfxExtInsertHeaders,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, SPS)
    FIELD_T(mfxU16, PPS)
)


STRUCT(mfxExtEncoderIPCMArea_area,
    FIELD_T(mfxU32, Left    )
    FIELD_T(mfxU32, Top     )
    FIELD_T(mfxU32, Right   )
    FIELD_T(mfxU32, Bottom  )
)

STRUCT(mfxExtEncoderIPCMArea,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16      , NumArea)
    FIELD_T(mfxExtEncoderIPCMArea_area*, Areas)
)

STRUCT(mfxExtChromaLocInfo,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16      , ChromaLocInfoPresentFlag)
    FIELD_T(mfxU16      , ChromaSampleLocTypeTopField)
    FIELD_T(mfxU16      , ChromaSampleLocTypeBottomField)
)

STRUCT(mfxExtDecodedFrameInfo,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16      , FrameType)
)

STRUCT(mfxExtDecodeErrorReport,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32      , ErrorTypes)
)

STRUCT(mfxInitParam,
    FIELD_T(mfxIMPL, Implementation)
    FIELD_S(mfxVersion, Version)
    FIELD_T(mfxU16, ExternalThreads)
    FIELD_T(mfxExtBuffer**, ExtParam)
    FIELD_T(mfxU16, NumExtParam)
    FIELD_T(mfxU16, GPUCopy)
)

STRUCT(mfxExtThreadsParam,
    FIELD_S(mfxExtBuffer, Header        )
    FIELD_T(mfxU16      , NumThread     )
    FIELD_T(mfxI32      , SchedulingType)
    FIELD_T(mfxI32      , Priority      )
)

STRUCT(mfxExtVPPFieldProcessing,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16      , Mode)
    FIELD_T(mfxU16      , InField)
    FIELD_T(mfxU16      , OutField)
)

STRUCT(mfxExtVPPRotation,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16      , Angle)
)

STRUCT(mfxExtVPPMirroring,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16      , Type)
)

STRUCT(mfxExtDirtyRect_Entry,
    FIELD_T(mfxU32, Left    )
    FIELD_T(mfxU32, Top     )
    FIELD_T(mfxU32, Right   )
    FIELD_T(mfxU32, Bottom  )
)

STRUCT(mfxExtDirtyRect,
    FIELD_S(mfxExtBuffer,          Header )
    FIELD_T(mfxI16,                NumRect)
    FIELD_S(mfxExtDirtyRect_Entry, Rect   )
)

STRUCT(mfxExtMoveRect_Entry,
    FIELD_T(mfxU32, DestLeft  )
    FIELD_T(mfxU32, DestTop   )
    FIELD_T(mfxU32, DestRight )
    FIELD_T(mfxU32, DestBottom)

    FIELD_T(mfxU32, SourceLeft)
    FIELD_T(mfxU32, SourceTop )
)

STRUCT(mfxExtMoveRect,
    FIELD_S(mfxExtBuffer,          Header )
    FIELD_T(mfxI16,                NumRect)
    FIELD_S(mfxExtMoveRect_Entry,  Rect   )
)

STRUCT(mfxExtMVCSeqDesc,
    FIELD_S(mfxExtBuffer          , Header        )
    FIELD_T(mfxU32                , NumView       )
    FIELD_T(mfxU32                , NumViewAlloc  )
  //FIELD_S(mfxMVCViewDependency* , View          )
    FIELD_T(mfxU32                , NumViewId     )
    FIELD_T(mfxU32                , NumViewIdAlloc)
  //FIELD_S(mfxU16*               , ViewId        )
    FIELD_T(mfxU32                , NumOP         )
    FIELD_T(mfxU32                , NumOPAlloc    )
  //FIELD_S(mfxMVCOperationPoint* , OP            )
    FIELD_T(mfxU16                , NumRefsTotal  )
)

STRUCT(mfxExtMBDisableSkipMap,
    FIELD_S(mfxExtBuffer, Header )
    FIELD_T(mfxU32      , MapSize)
)

STRUCT(mfxExtCodingOptionVPS,
    FIELD_S(mfxExtBuffer, Header    )
    FIELD_T(mfxU8*      , VPSBuffer )
    FIELD_T(mfxU16      , VPSBufSize)
    FIELD_T(mfxU16      , VPSId)
)


STRUCT(mfxExtHEVCParam,
    FIELD_S(mfxExtBuffer , Header                )
    FIELD_T(mfxU16       , PicWidthInLumaSamples )
    FIELD_T(mfxU16       , PicHeightInLumaSamples)
    FIELD_T(mfxU64       , GeneralConstraintFlags)
    FIELD_T(mfxU16       , SampleAdaptiveOffset)
    FIELD_T(mfxU16       , LCUSize)
)


STRUCT(mfxExtHEVCTiles,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, NumTileRows)
    FIELD_T(mfxU16, NumTileColumns)
)

STRUCT(mfxExtHEVCRegion,
    FIELD_S(mfxExtBuffer, Header )
    FIELD_T(mfxU32      , RegionId)
    FIELD_T(mfxU16      , RegionType)
    FIELD_T(mfxU16      , RegionEncoding)
)

STRUCT(mfxVP9SegmentParam,
    FIELD_T(mfxU16,  FeatureEnabled)
    FIELD_T(mfxI16,  QIndexDelta)
    FIELD_T(mfxI16,  LoopFilterLevelDelta)
    FIELD_T(mfxU16,  ReferenceFrame)
)

STRUCT(mfxExtVP9Segmentation,
    FIELD_S(mfxExtBuffer      , Header)
    FIELD_T(mfxU16            , NumSegments)
    FIELD_S(mfxVP9SegmentParam, Segment)
    FIELD_T(mfxU16            , SegmentIdBlockSize)
    FIELD_T(mfxU32            , NumSegmentIdAlloc)
    FIELD_T(mfxU8*            , SegmentId)
)

STRUCT(mfxVP9TemporalLayer,
    FIELD_T(mfxU16, FrameRateScale)
    FIELD_T(mfxU16, TargetKbps)
)

STRUCT(mfxExtVP9TemporalLayers,
    FIELD_S(mfxExtBuffer        , Header)
    FIELD_S(mfxVP9TemporalLayer , Layer)
)

STRUCT(mfxExtVP9Param,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16      , FrameWidth)
    FIELD_T(mfxU16      , FrameHeight)
    FIELD_T(mfxU16      , WriteIVFHeaders)
    FIELD_T(mfxI16      , QIndexDeltaLumaDC)
    FIELD_T(mfxI16      , QIndexDeltaChromaAC)
    FIELD_T(mfxI16      , QIndexDeltaChromaDC)
    FIELD_T(mfxU16      , NumTileRows)
    FIELD_T(mfxU16      , NumTileColumns)
)

STRUCT(mfxExtMBForceIntra,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32, MapSize)
    FIELD_T(mfxU8*, Map)
)

STRUCT(mfxExtMasteringDisplayColourVolume,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16      , InsertPayloadToggle)
    FIELD_T(mfxU16      , DisplayPrimariesX)
    FIELD_T(mfxU16      , DisplayPrimariesY)
    FIELD_T(mfxU16      , WhitePointX)
    FIELD_T(mfxU16      , WhitePointY)
    FIELD_T(mfxU32      , MaxDisplayMasteringLuminance)
    FIELD_T(mfxU32      , MinDisplayMasteringLuminance)
)

STRUCT(mfxExtContentLightLevelInfo,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16      , InsertPayloadToggle)
    FIELD_T(mfxU16      , MaxContentLightLevel)
    FIELD_T(mfxU16      , MaxPicAverageLightLevel)
)

STRUCT(mfxExtPredWeightTable,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, LumaLog2WeightDenom)
    FIELD_T(mfxU16, ChromaLog2WeightDenom)
    FIELD_T(mfxU16, LumaWeightFlag)
    FIELD_T(mfxU16, ChromaWeightFlag)
    FIELD_T(mfxI16, Weights)
)

#if defined(__MFXBRC_H__)
STRUCT(mfxExtBRC,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxHDL      , pthis)
    FIELD_T(mfxHDL      , Init)
    FIELD_T(mfxHDL      , Reset)
    FIELD_T(mfxHDL      , Close)
    FIELD_T(mfxHDL      , GetFrameCtrl)
    FIELD_T(mfxHDL      , Update)
)

STRUCT(mfxBRCFrameParam,
    FIELD_T(mfxU16        , SceneChange)
    FIELD_T(mfxU16        , LongTerm)
    FIELD_T(mfxU32        , FrameCmplx)
    FIELD_T(mfxU32        , EncodedOrder)
    FIELD_T(mfxU32        , DisplayOrder)
    FIELD_T(mfxU32        , CodedFrameSize)
    FIELD_T(mfxU16        , FrameType)
    FIELD_T(mfxU16        , PyramidLayer)
    FIELD_T(mfxU16        , NumRecode)
    FIELD_T(mfxU16        , NumExtParam)
    FIELD_T(mfxExtBuffer**, ExtParam)
)

STRUCT(mfxBRCFrameCtrl,
    FIELD_T(mfxI32        , QpY)
    FIELD_T(mfxU32        , InitialCpbRemovalDelay)
    FIELD_T(mfxU32        , InitialCpbRemovalOffset)
    FIELD_T(mfxU32        , reserved1)
    FIELD_T(mfxU32        , MaxFrameSize)
    FIELD_T(mfxU8         , DeltaQP)
    FIELD_T(mfxU16        , MaxNumRepak)
    FIELD_T(mfxU16        , NumExtParam)
    FIELD_T(mfxExtBuffer**, ExtParam)
)

STRUCT(mfxBRCFrameStatus,
    FIELD_T(mfxU32, MinFrameSize)
    FIELD_T(mfxU16, BRCStatus)
    FIELD_T(mfxU16, reserved)
    FIELD_T(mfxHDL, reserved1)
)

#endif // defined(__MFXBRC_H__)

STRUCT(mfxEncodedUnitInfo,
    FIELD_T(mfxU16, Type)
    FIELD_T(mfxU32, Offset)
    FIELD_T(mfxU32, Size)
)

STRUCT(mfxExtEncodedUnitsInfo,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxEncodedUnitInfo*, UnitInfo)
    FIELD_T(mfxU16, NumUnitsAlloc)
    FIELD_T(mfxU16, NumUnitsEncoded)
)

#if defined(__MFXPAVP_H__)

STRUCT(mfxExtCencParam,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32      , StatusReportIndex)
)

#endif // defined(__MFXPAVP_H__)

#if defined(__MFXSCD_H__)
STRUCT(mfxExtSCD,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, SceneType)
)
#endif // defined(__MFXSCD_H__)

STRUCT(mfxExtVppMctf,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, FilterStrength)
)

STRUCT(mfxAV1FilmGrainPoint,
    FIELD_T(mfxU8, Value)
    FIELD_T(mfxU8, Scaling)
)

STRUCT(mfxExtAV1FilmGrainParam,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, FilmGrainFlags)
    FIELD_T(mfxU16, GrainSeed)
    FIELD_T(mfxU8, RefIdx)
    FIELD_T(mfxI8, NumYPoints)
    FIELD_T(mfxI8, NumCbPoints)
    FIELD_T(mfxI8, NumCrPoints)
    FIELD_S(mfxAV1FilmGrainPoint, PointY)
    FIELD_S(mfxAV1FilmGrainPoint, PointCb)
    FIELD_S(mfxAV1FilmGrainPoint, PointCr)
    FIELD_T(mfxI8, GrainScalingMinus8)
    FIELD_T(mfxU8, ArCoeffLag)
    FIELD_T(mfxU8, ArCoeffsYPlus128)
    FIELD_T(mfxU8, ArCoeffsCbPlus128)
    FIELD_T(mfxU8, ArCoeffsCrPlus128)
    FIELD_T(mfxU8, ArCoeffShiftMinus6)
    FIELD_T(mfxU8, GrainScaleShift)
    FIELD_T(mfxU8, CbMult)
    FIELD_T(mfxU8, CbLumaMult)
    FIELD_T(mfxU16, CbOffset)
    FIELD_T(mfxU8, CrMult)
    FIELD_T(mfxU8, CrLumaMult)
    FIELD_T(mfxU16, CrOffset)
)

STRUCT(mfxExtAV1BitstreamParam,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, WriteIVFHeaders)
)

STRUCT(mfxExtAV1ResolutionParam,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32, FrameWidth)
    FIELD_T(mfxU32, FrameHeight)
)

STRUCT(mfxExtAV1TileParam,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, NumTileRows)
    FIELD_T(mfxU16, NumTileColumns)
    FIELD_T(mfxU16, NumTileGroups)
)

STRUCT(mfxAV1SegmentParam,
    FIELD_T(mfxU16, FeatureEnabled)
    FIELD_T(mfxU16, AltQIndex)
)

STRUCT(mfxExtAV1Segmentation,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU8, NumSegments)
    FIELD_S(mfxAV1SegmentParam, Segment)
    FIELD_T(mfxU16, SegmentIdBlockSize)
    FIELD_T(mfxU32, NumSegmentIdAlloc)
    FIELD_T(mfxU8*, SegmentIds)
)

STRUCT(mfxExtPartialBitstreamParam,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32, BlockSize)
    FIELD_T(mfxU16, Granularity)
    FIELD_T(mfxU16, reserved)
)

STRUCT(mfxExtVPPPercEncPrefilter,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, reserved)
)

#if defined(MFX_ENABLE_ENCTOOLS)
STRUCT(mfxExtEncToolsConfig,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_S(mfxStructVersion,  Version)
    FIELD_T(mfxU16, AdaptiveI)
    FIELD_T(mfxU16, AdaptiveB)
    FIELD_T(mfxU16, AdaptiveRefP)
    FIELD_T(mfxU16, AdaptiveRefB)
    FIELD_T(mfxU16, SceneChange)
    FIELD_T(mfxU16, AdaptiveLTR)
    FIELD_T(mfxU16, AdaptivePyramidQuantP)
    FIELD_T(mfxU16, AdaptivePyramidQuantB)
    FIELD_T(mfxU16, AdaptiveQuantMatrices)
    FIELD_T(mfxU16, BRCBufferHints)
    FIELD_T(mfxU16, BRC)
    FIELD_T(mfxU16, reserved) /* Fixed size array */
)
#endif

#ifdef MFX_ENABLE_ENCODE_STATS
STRUCT(mfxExtEncodeStatsOutput,
    FIELD_S(mfxExtBuffer, Header)
)
#endif

STRUCT(mfxExtTuneEncodeQuality,
    FIELD_S(mfxExtBuffer,   Header)
    FIELD_T(mfxU32,         TuneQuality)
    FIELD_T(mfxExtBuffer**, ExtParam)
    FIELD_T(mfxU16        , NumExtParam)
)

STRUCT(mfxExtQualityInfoMode,
    FIELD_S(mfxExtBuffer, Header)
)

STRUCT(mfxExtQualityInfoOutput,
    FIELD_S(mfxExtBuffer, Header)
)

STRUCT(mfxExtAV1ScreenContentTools,
    FIELD_S(mfxExtBuffer, Header)
)

#ifdef MFX_ENABLE_AI_ENC_CTRL
STRUCT(mfxExtAIEncCtrl,
    FIELD_S(mfxExtBuffer, Header)
)
#endif