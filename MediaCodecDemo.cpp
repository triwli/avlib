#include <iostream>
#include <cstring>

#include <stdlib.h> // atoi
#include <stdio.h> // fopen fclose

#include "MediaCodec.h"

using namespace std;

#define TMPOUT 1

int main(int argc, char **argv)
{
    int errCode = 0;
    int retCode = 0;

    const char  *srcFile = NULL;
    const char  *dstFile = NULL;
    int         iFrameNum = 0;
    void        *hDemux = NULL;
    void        *hMux = NULL;
    void        *vBuf = NULL;
    int         vBufSize = 0;

    MEDIAINFO_S mediaInfo;

    do {
        // validate the argument
        if ( 4 != argc ) {
            cout << "Usage: " << argv[0] << "srcFile dstFile FrameNum" << endl;
            break;
        }

        srcFile = argv[1];
        dstFile = argv[2];
        iFrameNum = atoi(argv[3]);

        // codec init
        errCode = codecInit();
        memset(&mediaInfo, 0, sizeof(MEDIAINFO_S));
        
        // decoder set with input file
        errCode = demuxerInit(&hDemux, &mediaInfo, srcFile);
        if ( errCode > 0 ) {
            cout << "DEMUXER Init Failed!" << endl;
            retCode = 21;
            break;
        }

        // encoder set with output file
        errCode = muxerInit(&hMux, &mediaInfo, dstFile);
        if ( errCode > 0 ) {
            cout << "MUXER Init Failed" << endl;
            retCode = 23;
            break;
        }

#if TMPOUT
        FILE *tmpFile = fopen("tmp.yuv", "wb");
        if ( NULL == tmpFile ) {
            cout << "Could not open tmp file" << endl;
            break;
        }

        int i = 0;
        for ( i = 0; i < iFrameNum; i++ ) {
            // get raw data from the decoder
            if ( 12 == i ) {
                cout << "Stream to be End" << endl;
            }

            errCode = demuxPacket(hDemux, &vBuf, &vBufSize);
            if ( errCode ) {
                break;
            }

            fwrite(vBuf, 1, vBufSize, tmpFile);
            errCode = muxPacket(hMux, vBuf, vBufSize);
            if ( errCode ) {
                break;
            }
        }
#endif
        
        // decoder and encoder deinit
        errCode = demuxerDeInit(hDemux);
        if ( errCode > 0 ) {
            cout << "DEMUXER DeInit Failed" << endl;
            retCode = 23;
            break;
        }

        errCode = muxerDeInit(hMux);
        if ( errCode > 0 ) {
            cout << "MUXER DeInit Failed" << endl;
            retCode = 23;
            break;
        }

#if TMPOUT
        if ( tmpFile ) {
            fclose(tmpFile);
        }
#endif
    } while ( 0 );

    return retCode;
}
