#include <iostream>
#include <cstring>

extern "C"
{
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

using namespace std;

typedef struct DEMUXER {
    AVFormatContext *iFmtCtx;
    AVStream        *iStream;
    AVCodecContext  *vDecCtx;
    AVCodec         *vDec;
    enum AVCodecID  vDecID;
    AVPacket        pkt;
    int             streamEnd;
    AVFrame         *dFrame;
    int             frameCached;
    uint8_t         *vRawData[4];
    int             vRawLineSize[4];
    int             vBufSize;
    int             vStreamIndex;
    int             width;
    int             height;
    enum AVPixelFormat  vPixFmt;
    AVRational      vTimebase;
} DEMUXER_S;

typedef struct MUXER {
    AVFormatContext *oFmtCtx;
    AVOutputFormat  *oFmt;
    AVStream        *oStream;
    AVCodecContext  *vEncCtx;
    AVCodec         *vEnc;
    enum AVCodecID  vEncID;
    AVPacket        pkt;
    AVFrame         *eFrame;
    struct SwsContext   *swCtx;
    uint8_t         *vRawData[4];
    int             vRawLineSize[4];
    int             frameNum;
    int             delayedFrameNum;
    int             vBufSize;
    int             width;
    int             height;
    enum AVPixelFormat  vPixFmt;
    AVRational          vTimebase; 
} MUXER_S;

#include "MediaCodec.h"

int decFrame(DEMUXER_S *hDemux);
int encFrame(MUXER_S *pMux);
int flushCachedPacket(MUXER_S *pMux);

int codecInit() 
{
    int retCode = 0;
    
    av_register_all();

    return retCode;
}

int demuxerDeInit(void *hDemux)
{
    //int errCode = 0;
    int retCode = 0;
    DEMUXER_S *pDemux = NULL;
    
    do {
        pDemux = (DEMUXER_S *) hDemux;
        avcodec_close(pDemux->vDecCtx);
        avformat_close_input(&(pDemux->iFmtCtx));
        av_frame_free(&(pDemux->dFrame));
        av_free(pDemux->vRawData[0]);
        free(pDemux);
    } while ( 0 );

    return retCode;
}

int muxerDeInit(void *hMux)
{
    //int errCode = 0;
    int retCode = 0;
    //int gotOutput;
    MUXER_S *pMux = NULL;

    do {
        pMux = (MUXER_S *) hMux;
        flushCachedPacket(pMux);
        av_write_trailer(pMux->oFmtCtx);
        avcodec_close(pMux->vEncCtx);
        av_free(pMux->vRawData[0]);
        
        if ( pMux->oFmtCtx && !(pMux->oFmtCtx->flags & AVFMT_NOFILE) ) {
            avio_close(pMux->oFmtCtx->pb);
        }

        avformat_free_context(pMux->oFmtCtx);
        if ( pMux->swCtx ) {
            sws_freeContext(pMux->swCtx);
        }

        free(pMux);
    } while ( 0 );

    return retCode;
}

int muxerInit(void **hhMux, MEDIAINFO_S *pMediaInfo,
            const char *dstFile)
{
    int retCode = 0;
    int errCode = 0;
    AVRational tmp;

    do {
        if ( (NULL == hhMux) || (NULL == pMediaInfo) || (NULL == dstFile) ) {
            cout << "Input Parameter Invalidate in muxerInit()" << endl;
            retCode = 0;
            break;
        }

        MUXER_S *pMux = (MUXER_S *) malloc(sizeof (MUXER_S));
        memset(pMux, 0, sizeof (MUXER_S));
        errCode = avformat_alloc_output_context2(&(pMux->oFmtCtx), NULL,
                                                 NULL, dstFile);
        if ( !(pMux->oFmtCtx) ) {
            cout << "Could not create output context" << endl;
            retCode = 11;
            break;
        }

        pMux->oFmt = pMux->oFmtCtx->oformat;
        if ( AV_CODEC_ID_NONE == pMux->oFmt->video_codec ) {
            cout << "Not Suitable Format" << endl;
            retCode = 13;
            break;
        }

        pMux->vEncID = pMux->oFmt->video_codec;
        if ( AV_CODEC_ID_MJPEG == pMux->vEncID ) {
            if ( strstr(dstFile, ".png") ) {
                pMux->vEncID = AV_CODEC_ID_PNG;
            } else if ( strstr(dstFile, ".bmp") ) {
                pMux->vEncID = AV_CODEC_ID_BMP;
            }
        } else {
            pMux->vEncID = AV_CODEC_ID_H264;
        }

        pMux->vEnc = avcodec_find_encoder(pMux->vEncID);
        if ( !(pMux->vEnc) ) {
            cout << "Necessary Encoder not found" << endl;
            retCode = 15;
            break;
        }

        pMux->oStream = avformat_new_stream(pMux->oFmtCtx, pMux->vEnc);
        if ( !(pMux->oStream) ) {
            cout << "Failed allocating output stream" << endl;
            retCode = 17;
            break;
        }

        pMux->oStream->id = pMux->oFmtCtx->nb_streams - 1;
        pMux->vEncCtx = pMux->oStream->codec;

        pMux->vEncCtx->codec_id = pMux->vEncID;
        pMux->width = pMediaInfo->width;
        pMux->vEncCtx->width = pMediaInfo->width;
        pMux->height = pMediaInfo->height;
        pMux->vEncCtx->height = pMediaInfo->height;
        pMux->vPixFmt = (enum AVPixelFormat) pMediaInfo->pixFmt;

        if (  AV_CODEC_ID_H264 == pMux->vEncID ) {
            pMux->vEncCtx->pix_fmt = AV_PIX_FMT_YUV420P;
            tmp.num = pMediaInfo->frame.num;
            tmp.den = pMediaInfo->frame.den;
            av_opt_set(pMux->vEncCtx->priv_data, "preset", "slow", 0);
        } else {
            tmp.num = 1;
            tmp.den = 25;
            if ( AV_CODEC_ID_MJPEG == pMux->vEncID ) {
                pMux->vEncCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
            } else {
                pMux->vEncCtx->pix_fmt = AV_PIX_FMT_RGB24;
            }
        }

        pMux->vTimebase = tmp;
        pMux->vEncCtx->time_base = tmp;
        pMux->oStream->time_base = tmp;
        if ( pMux->oFmtCtx->flags & AVFMT_GLOBALHEADER ) {
            pMux->oFmtCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        errCode = avcodec_open2(pMux->vEncCtx, pMux->vEnc, NULL);
        if ( errCode < 0 ) {
            cout << "Cannot open video encoder" << endl;
        }

        if ( errCode < 0 ) {
            cout << "Cannot open video encoder" << endl;
            retCode = 19;
            break;
        }

        pMux->eFrame = av_frame_alloc();
        if ( !(pMux->eFrame) ) {
            cout << "Could not allocate video Frame" << endl;
            retCode = 16;
            break;
        }

        pMux->eFrame->format = (int) pMux->vEncCtx->pix_fmt;
        pMux->eFrame->width = pMediaInfo->width;
        pMux->eFrame->height = pMediaInfo->height;
        
        errCode = av_image_alloc(pMux->eFrame->data, pMux->eFrame->linesize,
                                 pMux->eFrame->width, pMux->eFrame->height, 
                                 (enum AVPixelFormat) pMux->eFrame->format, 32);
        if ( errCode < 0 ) {
            cout << "Could not allocated raw picture buffer" << endl;
            retCode = 14;
            break;
        }

        pMux->vBufSize = av_image_alloc(pMux->vRawData, pMux->vRawLineSize,
                                        pMux->width, pMux->height,
                                        pMux->vPixFmt, 1);
        if ( pMux->vBufSize < 0 ) {
            cout << "Allocate Raw Buffer Error" << endl;
            retCode = 14;
            break;
        }

        av_dump_format(pMux->oFmtCtx, 0, dstFile, 1);
        if ( !(pMux->oFmtCtx->flags & AVFMT_NOFILE) ) {
            errCode = avio_open(&(pMux->oFmtCtx->pb), dstFile, 
                                AVIO_FLAG_WRITE);
            if ( errCode < 0 ) {
                cout << "could not open output file" << endl;
                retCode = 12;
                break;
            }
        }

        errCode = avformat_write_header(pMux->oFmtCtx, NULL);
        if ( errCode < 0 ) {
            cout << "Error occurred when opennning output file" << endl;
            retCode = 18;
            break;
        }

        if ( pMux->vPixFmt != pMux->vEncCtx->pix_fmt ) {
            pMux->swCtx = sws_getContext(pMux->width, pMux->height, pMux->vPixFmt,
                                         pMux->width, pMux->height, pMux->vEncCtx->pix_fmt,
                                         SWS_BICUBIC, NULL, NULL, NULL);
            if ( !(pMux->swCtx) ) {
                cout << " could not create SW Context" << endl;
                retCode = 16;
                break;
            }
        }

        *hhMux = (void *) pMux;
    } while ( 0 );

    return retCode;
}

int demuxerInit(void **hhDemux, MEDIAINFO *pMediaInfo, const char *srcFile)
{
    int retCode = 0;
    int errCode = 0;

    do {
        if ( (NULL==hhDemux) || (NULL==pMediaInfo) || (NULL==srcFile) ) {
            cout << "Input parameter Invalidate in demuxerInit()" << endl;
            retCode = 4;
            break;
        }

        DEMUXER_S *pDemux = (DEMUXER_S *) malloc(sizeof (DEMUXER_S));
        memset(pDemux, 0, sizeof(DEMUXER_S));

        errCode = avformat_open_input(&(pDemux->iFmtCtx), srcFile, 0, 0);
        if ( 0 != errCode ) {
            cout << "Could not open Source File" << srcFile << endl;
            retCode = 1;
            break;
        } 

        errCode = avformat_find_stream_info(pDemux->iFmtCtx, 0);
        if ( errCode < 0 ) {
            cout << "Failed to retrived input stream information" << endl;
            retCode = 3;
            break;
        }

        errCode = av_find_best_stream(pDemux->iFmtCtx, AVMEDIA_TYPE_VIDEO, 
                                      -1, -1, NULL, 0);
        if ( errCode < 0 ) {
            cout << "Could not find video stream in Source File" << endl;
            retCode = 5;
            break;
        }

        pDemux->vStreamIndex = errCode;
        pDemux->iStream = pDemux->iFmtCtx->streams[pDemux->vStreamIndex];
        pDemux->vDecCtx = pDemux->iStream->codec;
        pDemux->vDec = avcodec_find_decoder(pDemux->vDecCtx->codec_id);
        if ( !(pDemux->vDec) ) {
            cout << "Failed to find Video decoder" << endl;
            retCode = 7;
            break;
        }

        errCode = avcodec_open2(pDemux->vDecCtx, pDemux->vDec, NULL);
        if ( errCode < 0 ) {
            cout << "Failed to open video decoder" << endl;
            retCode = 8;
            break;
        }

        pDemux->width = pDemux->vDecCtx->width;
        pDemux->height = pDemux->vDecCtx->height;
        pDemux->vPixFmt = pDemux->vDecCtx->pix_fmt;
        AVRational tmp = { pDemux->iStream->avg_frame_rate.den,
                           pDemux->iStream->avg_frame_rate.num};
        pDemux->vTimebase = tmp;
        pDemux->vBufSize = av_image_alloc(pDemux->vRawData, pDemux->vRawLineSize, 
                                          pDemux->width, pDemux->height,
                                          pDemux->vPixFmt, 1);
        if ( pDemux->vBufSize < 0 ) {
            cout << "Could not allocate raw video buffer" << endl;
            retCode = 9;
            break;
        }

        pDemux->dFrame = av_frame_alloc();
        if ( !(pDemux->dFrame) ) {
            cout << "Could not allocate frame" << endl;
            retCode = 6;
            break;
        }

        av_init_packet(&(pDemux->pkt));
        pDemux->pkt.data = NULL;
        pDemux->pkt.size = 0;
        pMediaInfo->width = pDemux->width;
        pMediaInfo->height = pDemux->height;
        pMediaInfo->pixFmt = (PIXELFORMAT_E) pDemux->vPixFmt;
        pMediaInfo->frame.den = tmp.den;
        pMediaInfo->frame.num = tmp.num;
        
        *hhDemux = (void *) pDemux;
 
    } while ( 0 );

    return retCode;
}

int flushCachedPacket(MUXER_S *pMux)
{
    int errCode = 0;
    int retCode = 0;
    //int gotOutput = 0;

    do {
        av_free(pMux->eFrame->data[0]);
        av_frame_free(&(pMux->eFrame));

        if ( pMux->delayedFrameNum ) {
            while ( 1 ) {
                pMux->eFrame = NULL;
                errCode = encFrame(pMux);
                if ( 57 == errCode ) {
                    if ( 0 == pMux->delayedFrameNum ) {
                        break;
                    } else {
                        pMux->delayedFrameNum--;
                    }
                }
            }
        }
    } while ( 0 );

    return retCode;
}

int encFrame(MUXER_S *pMux)
{
    int errCode = 0;
    int retCode = 0;
    int gotOutput = 0;

    do {
        av_init_packet(&(pMux->pkt));
        pMux->pkt.data = NULL;
        pMux->pkt.size = 0;

        errCode = avcodec_encode_video2(pMux->vEncCtx, &(pMux->pkt),
                                        pMux->eFrame, &gotOutput);
        if ( errCode < 0 ) {
            cout << "Error encoding video frame" << endl;
            retCode = 53;
            break;
        }

        if ( gotOutput ) {
            av_packet_rescale_ts(&(pMux->pkt), pMux->vEncCtx->time_base, 
                                 pMux->oStream->time_base);
            pMux->pkt.stream_index = pMux->oStream->index;
            av_interleaved_write_frame(pMux->oFmtCtx, &(pMux->pkt));
        } else {
            retCode = 57;
        }
    } while ( 0 );

    return retCode;
}

int muxPacket(void *hMux, void *vBuf, int vBufSize)
{
    int errCode = 0;
    int retCode = 0;

    MUXER_S *pMux = NULL;

    do {
        pMux = (MUXER_S *) hMux;
        if ( vBufSize <= 0 ) {
            cout << "None Buffer Coming" << endl;
            retCode = 63;
            break;
        }

        memcpy(pMux->vRawData[0], vBuf, vBufSize);
        if ( pMux->vPixFmt == (enum AVPixelFormat) pMux->eFrame->format ) {
            av_image_copy((uint8_t **) (pMux->eFrame->data), 
                          pMux->eFrame->linesize, 
                          (const uint8_t **) pMux->vRawData, 
                          pMux->vRawLineSize,
                          (enum AVPixelFormat) pMux->eFrame->format,
                          pMux->width, pMux->height);
        } else {
            sws_scale(pMux->swCtx, (const uint8_t *const *)pMux->vRawData, 
                      pMux->vRawLineSize, 0, pMux->height, 
                      (uint8_t **) (pMux->eFrame->data), 
                      pMux->eFrame->linesize);
        }

        pMux->eFrame->pts = pMux->frameNum;
        pMux->frameNum++;
        errCode = encFrame(pMux);
        if ( 57 == errCode ) {
            pMux->delayedFrameNum++;
        } else if ( errCode ) {
            cout << "Mux Packet error" << endl;
            retCode = 61;
            break;
        }
    } while ( 0 );

    return retCode;
}

int demuxPacket(void *hDemux, void **vBuf, int *vBufSize)
{
    int errCode = 0;
    int retCode = 0;

    DEMUXER_S *pDemux = NULL;

    do {
        pDemux = (DEMUXER_S *) hDemux;
        do {
            if ( 0 == pDemux->streamEnd ) {
                if ( 0 == pDemux->frameCached ) {
                    if ( pDemux->pkt.size <= 0 ) {
                        errCode = av_read_frame(pDemux->iFmtCtx, 
                                                &(pDemux->pkt));
                        if ( errCode < 0 ) {
                            pDemux->frameCached = 1;
                        }
                    } else if ( pDemux->pkt.size > 0 ) {
                        if ( pDemux->pkt.stream_index == pDemux->vStreamIndex) {
                            int pktSize = pDemux->pkt.size;
                            errCode = decFrame(pDemux);
                            pDemux->pkt.data += pktSize;
                            pDemux->pkt.size -= pktSize;
                            if ( 0 == errCode ) {
                                *vBuf = pDemux->vRawData[0];
                                *vBufSize = pDemux->vBufSize;
                                break;
                            } else if ( 47 == errCode ) {
                                // cout << "Decoded Frame Cached" << endl;
                            } else if ( 45 == errCode ) {
                                *vBuf = NULL;
                                *vBufSize = 0;
                                retCode = 31;
                                cout << "Decode Error" << endl;
                                break;
                            }
                        } else {
                            int pktSize = pDemux->pkt.size;
                            pDemux->pkt.data += pktSize;
                            pDemux->pkt.size -= pktSize;
                        }
                    }
                } else if ( 1 == pDemux->frameCached ) {
                    pDemux->pkt.data = NULL;
                    pDemux->pkt.size = 0;
                    if ( pDemux->pkt.stream_index == pDemux->vStreamIndex ) {
                        errCode = decFrame(pDemux);
                        if ( 0 == errCode ) {
                            *vBuf = pDemux->vRawData[0];
                            *vBufSize = pDemux->vBufSize;
                            break;
                        } else if ( 47 == errCode ) {
                            // cout << "stream End" << endl;
                            pDemux->streamEnd = 1;
                        } else if ( errCode ) {
                            *vBuf = NULL;
                            *vBufSize = 0;
                            retCode = 31;
                            cout << "Decode Error" << endl;
                            break;
                        }
                    }
                }
            } else if ( 1 == pDemux->streamEnd ) {
                *vBuf = NULL;
                *vBufSize = 0;
                retCode = 37;
                break;
            }
        } while ( 1 );
    } while ( 0 );

    return retCode;
}

int decFrame(DEMUXER_S *pDemux)
{
    int errCode = 0;
    int retCode = 0;
    int gotFrame = 0;

    do {
        av_packet_rescale_ts(&(pDemux->pkt), pDemux->iStream->time_base, 
                             pDemux->vDecCtx->time_base);
        errCode = avcodec_decode_video2(pDemux->vDecCtx, pDemux->dFrame, 
                                        &gotFrame, &(pDemux->pkt));
        if ( errCode < 0 ) {
            cout << "Error decoding video frame" << endl;
            retCode = 0;
            break;
        }

        if ( gotFrame ) {
            if ( (pDemux->width != pDemux->dFrame->width)
              || (pDemux->height != pDemux->dFrame->height) 
              || (pDemux->vPixFmt != pDemux->dFrame->format) ) {
                cout << "Error: Width, Height and Pixel Format have Changed!" << endl;
                retCode = 45;
                break;
            }

            av_image_copy(pDemux->vRawData, pDemux->vRawLineSize, 
                          (const uint8_t **) (pDemux->dFrame->data), 
                          pDemux->dFrame->linesize, pDemux->vPixFmt, 
                          pDemux->width, pDemux->height);
        } else {
            retCode = 47;
        }
    } while ( 0 );

    return retCode;
}
