typedef enum PIXELFORMAT {
    PIXELFORMAT_NONE = -1,
    PIXELFORMAT_YUV420P = 0, 
    PIXELFORMAT_RGB24 = 2
} PIXELFORMAT_E;

typedef struct FRAMERATE {
    int num;
    int den;
} FRAMERATE_S;

typedef struct MEDIAINFO {
    int width;
    int height;
    PIXELFORMAT_E pixFmt;
    FRAMERATE_S frame;
} MEDIAINFO_S;

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

int codecInit();
int demuxerInit(void **hhDemux, MEDIAINFO_S *pMediaInfo,
              const char *srcFile);
int muxerInit(void **hhMux, MEDIAINFO_S *pMediaInfo,
            const char *dstFile);

int demuxPacket(void *hDemux, void **vBuf, int *vBufSize);
int muxPacket(void *hMux, void *vBuf, int vBufSize);
int demuxerDeInit(void *hDemux);
int muxerDeInit(void *hMux);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
