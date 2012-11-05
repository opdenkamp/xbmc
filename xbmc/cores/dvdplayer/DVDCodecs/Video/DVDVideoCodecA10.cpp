#include "DVDVideoCodecA10.h"
#include "DVDClock.h"
#include "utils/log.h"
#include "threads/Atomics.h"
#include "../VideoRenderers/LinuxRendererA10.h"
//check for eden
#include "../VideoRenderers/RenderFlags.h"
#ifdef CONF_FLAGS_FORMAT_A10BUF
#define RENDER_FMT_YUV420P      DVDVideoPicture::FMT_YUV420P
#define RENDER_FMT_A10BUF       DVDVideoPicture::FMT_A10BUF
#endif

#include <sys/ioctl.h>
#include <math.h>

extern "C" {
#include "os_adapter.h"
};

static long g_cedaropen = 0;

#define A10DEBUG
#define MEDIAINFO

/*Cedar Decoder*/
//#define A10ENABLE_MPEG1       //does not like my tmpgenc videos
#define A10ENABLE_MPEG2         //ok
#define A10ENABLE_H264          //ok
//#define A10ENABLE_H263        //fails
#define A10ENABLE_VC1_WVC1      //ok
#define A10ENABLE_VP6           //ok
#define A10ENABLE_VP8           //ok
//#define A10ENABLE_FLV1        //fails
#define A10ENABLE_MJPEG         //ok
#define A10ENABLE_WMV1          //ok
//#define A10ENABLE_WMV2        //fails
#define A10ENABLE_WMV3          //ok
//#define A10ENABLE_MPEG4V1
//#define A10ENABLE_MPEG4V2
//#define A10ENABLE_MPEG4V3     //ok for divx3
//#define A10ENABLE_DIVX4
//#define A10ENABLE_DIVX5
//#define A10ENABLE_XVID          //mostly ok

/*
TODO:- Finish adding MPEG4 codecs tags 
     - Find out whats causing problems with several codecs, something is wrong or missing in the maping.
     - Add RealVideo once .rm files can be opened.
     - AVS and RMG2 codec support.

Note: AllWinner doc says to add FLV container type to VP6 and FLV1, but if i do so they stop working.
*/

#define _4CC(c1,c2,c3,c4) (((u32)(c4)<<24)|((u32)(c3)<<16)|((u32)(c2)<<8)|(u32)(c1))

static void freecallback(void *callbackpriv, void *pictpriv, cedarv_picture_t &pict)
{
  ((CDVDVideoCodecA10*)callbackpriv)->FreePicture(pictpriv, pict);
}

CDVDVideoCodecA10::CDVDVideoCodecA10()
{
  m_hcedarv  = NULL;
  m_yuvdata  = NULL;
  m_hwrender = false;
  memset(&m_picture, 0, sizeof(m_picture));
}

CDVDVideoCodecA10::~CDVDVideoCodecA10()
{
  Dispose();
}

/*
 * Open the decoder, returns true on success
 */
bool CDVDVideoCodecA10::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  if (getenv("NOA10"))
  {
    CLog::Log(LOGNOTICE, "A10: disabled.\n");
    return false;
  }

  if (hints.software)
  {
    CLog::Log(LOGNOTICE, "A10: software decoding requested.\n");
    return false;
  }
  else
  {
#ifdef TARGET_ANDROID
    m_hwrender = false;
#else
    m_hwrender = getenv("A10HWR") != NULL;
#endif
  }

  CLog::Log(LOGNOTICE, "A10: using %s rendering.\n", m_hwrender ? "hardware" : "software");

  m_hints  = hints;
  m_aspect = m_hints.aspect;

  memset(&m_info, 0, sizeof(m_info));
  //m_info.frame_rate       = (double)m_hints.fpsrate / m_hints.fpsscale * 1000;
  m_info.frame_duration = 0;
  m_info.video_width = m_hints.width;
  m_info.video_height = m_hints.height;
  m_info.aspect_ratio = 1000;
  m_info.sub_format = CEDARV_SUB_FORMAT_UNKNOW;
  m_info.container_format = CEDARV_CONTAINER_FORMAT_UNKNOW;
  m_info.init_data_len = 0;
  m_info.init_data = NULL;

#ifdef MEDIAINFO
  CLog::Log(LOGDEBUG, "A10: MEDIAINFO: CodecID %d \n", m_hints.codec);
  CLog::Log(LOGDEBUG, "A10: MEDIAINFO: StreamType %d \n", m_hints.type);
  CLog::Log(LOGDEBUG, "A10: MEDIAINFO: Level %d \n", m_hints.level);
  CLog::Log(LOGDEBUG, "A10: MEDIAINFO: Profile %d \n", m_hints.profile);
  CLog::Log(LOGDEBUG, "A10: MEDIAINFO: PTS_invalid %d \n", m_hints.ptsinvalid);
  CLog::Log(LOGDEBUG, "A10: MEDIAINFO: Tag %d \n", m_hints.codec_tag);
  { u8 *pb = (u8*)&m_hints.codec_tag;
    if (isalnum(pb[0]) && isalnum(pb[1]) && isalnum(pb[2]) && isalnum(pb[3]))
      CLog::Log(LOGDEBUG, "A10: MEDIAINFO: Tag fourcc %c%c%c%c\n", pb[0], pb[1], pb[2], pb[3]);
  }
#endif

  switch(m_hints.codec)
  {
  //MPEG1
#ifdef A10ENABLE_MPEG1
  case CODEC_ID_MPEG1VIDEO:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG2;
    m_info.sub_format = CEDARV_MPEG2_SUB_FORMAT_MPEG1;
    break;
#endif
    //MPEG2
#ifdef A10ENABLE_MPEG2
  case CODEC_ID_MPEG2VIDEO:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG2;
    m_info.sub_format = CEDARV_MPEG2_SUB_FORMAT_MPEG2;
    break;
#endif
    //H263
#ifdef A10ENABLE_H263
  case CODEC_ID_H263:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG4;
    m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_H263;
    break;
#endif
    //H264
#ifdef A10ENABLE_H264
  case CODEC_ID_H264:
    m_info.format = CEDARV_STREAM_FORMAT_H264;
    m_info.init_data_len = m_hints.extrasize;
    m_info.init_data = (u8*)m_hints.extradata;
    if(m_hints.codec_tag==27) //M2TS and TS
      m_info.container_format = CEDARV_CONTAINER_FORMAT_TS;
    break;
#endif
    //VP6
#ifdef A10ENABLE_VP6
  case CODEC_ID_VP6F:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG4;
    m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_VP6;
    //m_info.container_format = CEDARV_CONTAINER_FORMAT_FLV;
    break;
#endif
    //WMV1
#ifdef A10ENABLE_WMV1
  case CODEC_ID_WMV1:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG4;
    m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_WMV1;
    break;
#endif
    //WMV2
#ifdef A10ENABLE_WMV2
  case CODEC_ID_WMV2:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG4;
    m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_WMV2;
    break;
#endif
    //WMV3
#ifdef A10ENABLE_WMV3
  case CODEC_ID_WMV3:
    m_info.format = CEDARV_STREAM_FORMAT_VC1;
    m_info.init_data_len = m_hints.extrasize;
    m_info.init_data = (u8*)m_hints.extradata;
    break;
#endif
    //VC1 and WVC1
#ifdef A10ENABLE_VC1_WVC1
  case CODEC_ID_VC1:
    m_info.format = CEDARV_STREAM_FORMAT_VC1;
    m_info.init_data_len = m_hints.extrasize;
    m_info.init_data = (u8*)m_hints.extradata;
    break;
#endif
    //MJPEG
#ifdef A10ENABLE_MJPEG
  case CODEC_ID_MJPEG:
    m_info.format = CEDARV_STREAM_FORMAT_MJPEG;
    m_info.init_data_len = m_hints.extrasize;
    m_info.init_data = (u8*)m_hints.extradata;
    break;
#endif
    //VP8
#ifdef A10ENABLE_VP8
  case CODEC_ID_VP8:
    m_info.format = CEDARV_STREAM_FORMAT_VP8;
    m_info.init_data_len = m_hints.extrasize;
    m_info.init_data = (u8*)m_hints.extradata;
    break;
#endif
    //MSMPEG4V1
#ifdef A10ENABLE_MPEG4V1
  case CODEC_ID_MSMPEG4V1:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG4;
    m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_DIVX1;
    break;
#endif
    //MSMPEG4V2
#ifdef A10ENABLE_MPEG4V2
  case CODEC_ID_MSMPEG4V2:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG4;
    m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_DIVX2;
    break;
#endif
    //MSMPEG4V3
#ifdef A10ENABLE_MPEG4V3
  case CODEC_ID_MSMPEG4V3:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG4;
    m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_DIVX3;
    break;
#endif
    //Sorensson Spark (FLV1)
#ifdef A10ENABLE_FLV1
  case CODEC_ID_FLV1:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG4;
    m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_SORENSSON_H263;
    //m_info.container_format = CEDARV_CONTAINER_FORMAT_FLV;
    break;
#endif
    //Detected as MPEG4 (ID 13)
  case CODEC_ID_MPEG4:
    m_info.format = CEDARV_STREAM_FORMAT_MPEG4;
    switch(m_hints.codec_tag)
    {
    //DX40/DIVX4, divx
#ifdef A10ENABLE_DIVX4
    case _4CC('D','I','V','X'):
      m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_DIVX4;
      break;
#endif
    //DX50/DIVX5
#ifdef A10ENABLE_DIVX5
    case _4CC('D','X','5','0'):
    case _4CC('D','I','V','5'):
      m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_DIVX5;
      break;
#endif
   //XVID
#ifdef A10ENABLE_XVID
    case _4CC('X','V','I','D'):
    case _4CC('M','P','4','V'):
    case _4CC('P','M','P','4'):
    case _4CC('F','M','P','4'):
      m_info.sub_format = CEDARV_MPEG4_SUB_FORMAT_XVID;
      break;
#endif
    default:
      CLog::Log(LOGERROR, "A10: (MPEG4)Codec Tag %d is unknown.\n", m_hints.codec_tag);
      return false;
    }
    break;

  default:
    CLog::Log(LOGERROR, "A10: codecid %d is unknown.\n", m_hints.codec);
    return false;
  }

  return DoOpen();
}

bool CDVDVideoCodecA10::DoOpen()
{
  s32 ret;

  if (cas(&g_cedaropen, 0, 1) != 0)
  {
    CLog::Log(LOGERROR, "A10: cedar already in use");
    return false;
  }

  m_hcedarv = libcedarv_init(&ret);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "A10: libcedarv_init failed. (%d)\n", ret);
    goto Error;
  }

  ret = m_hcedarv->set_vstream_info(m_hcedarv, &m_info);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "A10: set_vstream_info failed. (%d)\n", ret);
    goto Error;
  }

  ret = m_hcedarv->open(m_hcedarv);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "A10: open failed. (%d)\n", ret);
    goto Error;
  }

  ret = m_hcedarv->ioctrl(m_hcedarv, CEDARV_COMMAND_PLAY, 0);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "A10: CEDARV_COMMAND_PLAY failed. (%d)\n", ret);
    goto Error;
  }

  CLog::Log(LOGDEBUG, "A10: cedar open.");
  return true;

Error:

  Dispose();
  return false;
}

/*
 * Dispose, Free all resources
 */
void CDVDVideoCodecA10::Dispose()
{
  A10VLFreeQueue();
  if (m_yuvdata)
  {
    mem_pfree(m_yuvdata);
    m_yuvdata = NULL;
  }
  if (m_hcedarv)
  {
    m_hcedarv->ioctrl(m_hcedarv, CEDARV_COMMAND_STOP, 0);
    m_hcedarv->close(m_hcedarv);
    libcedarv_exit(m_hcedarv);
    m_hcedarv = NULL;
    cas(&g_cedaropen, 1, 0);
    CLog::Log(LOGDEBUG, "A10: cedar dispose.");
  }
}

/*
 * returns one or a combination of VC_ messages
 * pData and iSize can be NULL, this means we should flush the rest of the data.
 */
int CDVDVideoCodecA10::Decode(BYTE* pData, int iSize, double dts, double pts)
{
  s32                        ret;
  u8                        *buf0, *buf1;
  u32                        bufsize0, bufsize1;
  cedarv_stream_data_info_t  dinf;
  cedarv_picture_t           picture;

  if (!pData)
    return VC_BUFFER;

  if (!m_hcedarv)
    return VC_ERROR;

  ret = m_hcedarv->request_write(m_hcedarv, iSize, &buf0, &bufsize0, &buf1, &bufsize1);
  if(ret < 0)
  {
    CLog::Log(LOGERROR, "A10: request_write failed.\n");
    return VC_ERROR;
  }
  if (bufsize1)
  {
    memcpy(buf0, pData, bufsize0);
    memcpy(buf1, pData+bufsize0, bufsize1);
  }
  else
  {
    memcpy(buf0, pData, iSize);
  }

  memset(&dinf, 0, sizeof(dinf));
  dinf.lengh = iSize;
#ifdef CEDARV_FLAG_DECODE_NO_DELAY
  dinf.flags = CEDARV_FLAG_FIRST_PART | CEDARV_FLAG_LAST_PART | CEDARV_FLAG_DECODE_NO_DELAY;
#else
  dinf.flags = CEDARV_FLAG_FIRST_PART | CEDARV_FLAG_LAST_PART;
#endif
  m_hcedarv->update_data(m_hcedarv, &dinf);

  ret = m_hcedarv->decode(m_hcedarv);

  if (ret > 3 || ret < 0)
  {
    CLog::Log(LOGERROR, "A10: decode(%d): %d\n", iSize, ret);
  }

  if (ret == 4)
  {
    CLog::Log(LOGNOTICE, "A10: Out of decoder frame buffers. Freeing the queue.\n");

    // DvdPlayer is dropping/queueing more frames then libcedarv has
    // frame buffers. Free the decoder frame queue.
    // The next few frames in the display queue will get invalidated,
    // but better than stopping the flow.
    A10VLFreeQueue();

    m_hcedarv->decode(m_hcedarv);
  }

  ret = m_hcedarv->display_request(m_hcedarv, &picture);

  if (ret > 3 || ret < -1)
  {
    CLog::Log(LOGERROR, "A10: display_request(%d): %d\n", iSize, ret);
  }

  if (ret == 0)
  {
    float aspect_ratio = m_aspect;

    m_picture.pts     = pts;
    m_picture.dts     = dts;
    m_picture.iWidth  = picture.display_width;
    m_picture.iHeight = picture.display_height;

    if (picture.is_progressive) m_picture.iFlags &= ~DVP_FLAG_INTERLACED;
    else                        m_picture.iFlags |= DVP_FLAG_INTERLACED;

    /* XXX: we suppose the screen has a 1.0 pixel ratio */ // CDVDVideo will compensate it.
    if (aspect_ratio <= 0.0)
      aspect_ratio = (float)m_picture.iWidth / (float)m_picture.iHeight;

    m_picture.iDisplayHeight = m_picture.iHeight;
    m_picture.iDisplayWidth  = ((int)lrint(m_picture.iHeight * aspect_ratio)) & -3;
    if (m_picture.iDisplayWidth > m_picture.iWidth)
    {
      m_picture.iDisplayWidth  = m_picture.iWidth;
      m_picture.iDisplayHeight = ((int)lrint(m_picture.iWidth / aspect_ratio)) & -3;
    }

    if (m_hwrender)
    {

      m_picture.format     = RENDER_FMT_A10BUF;
      m_picture.a10buffer  = A10VLPutQueue(freecallback, (void*)this, NULL, picture);
      m_picture.iFlags    |= DVP_FLAG_ALLOCATED;

      //CLog::Log(LOGDEBUG, "A10: decode %d\n", buffer->picture.id);
    }
    else
    {
      A10VLScalerParameter cdx_scaler_para;
      u32 width32;
      u32 height32;
      u32 height64;
      u32 ysize;
      u32 csize;

      m_picture.format = RENDER_FMT_YUV420P;

      width32  = (picture.display_width  + 31) & ~31;
      height32 = (picture.display_height + 31) & ~31;
      height64 = (picture.display_height + 63) & ~63;

      ysize = width32*height32;   //* for y.
      csize = width32*height64/2; //* for u and v together.

      if (!m_yuvdata) {
        m_yuvdata = (u8*)mem_palloc(ysize + csize, 1024);
        if (!m_yuvdata) {
          CLog::Log(LOGERROR, "A10: can not alloc m_yuvdata!");
          m_hcedarv->display_release(m_hcedarv, picture.id);
          return VC_ERROR;
        }
      }

      cdx_scaler_para.width_in   = picture.display_width;
      cdx_scaler_para.height_in  = picture.display_height;
      cdx_scaler_para.addr_y_in  = mem_get_phy_addr((u32)picture.y);
      cdx_scaler_para.addr_c_in  = mem_get_phy_addr((u32)picture.u);
      cdx_scaler_para.width_out  = picture.display_width;
      cdx_scaler_para.height_out = picture.display_height;
      cdx_scaler_para.addr_y_out = mem_get_phy_addr((u32)m_yuvdata);
      cdx_scaler_para.addr_u_out = cdx_scaler_para.addr_y_out + ysize;
      cdx_scaler_para.addr_v_out = cdx_scaler_para.addr_u_out + csize/2;

      if (!(m_picture.iFlags & DVP_FLAG_ALLOCATED)) {
        u32 width16  = (picture.display_width  + 15) & ~15;

        m_picture.iFlags |= DVP_FLAG_ALLOCATED;

        m_picture.iLineSize[0] = width16;   //Y
        m_picture.iLineSize[1] = width16/2; //U
        m_picture.iLineSize[2] = width16/2; //V
        m_picture.iLineSize[3] = 0;

        m_picture.data[0] = m_yuvdata;
        m_picture.data[1] = m_yuvdata+ysize;
        m_picture.data[2] = m_yuvdata+ysize+csize/2;

#ifdef A10DEBUG
        CLog::Log(LOGDEBUG, "A10: p1=%d %d %d %d (%d)\n", picture.width, picture.height, picture.display_width, picture.display_height, picture.pixel_format);
        CLog::Log(LOGDEBUG, "A10: p2=%d %d %d %d\n", m_picture.iWidth, m_picture.iHeight, m_picture.iDisplayWidth, m_picture.iDisplayHeight);
#endif
      }

      if (!A10VLPictureScaler(&cdx_scaler_para))
      {
        CLog::Log(LOGERROR, "A10: hardware scaler failed.\n");
        m_hcedarv->display_release(m_hcedarv, picture.id);
        return VC_ERROR;
      }

      m_hcedarv->display_release(m_hcedarv, picture.id);
    }

    return VC_PICTURE | VC_BUFFER;
  }

  return VC_BUFFER;
}

/*
 * Reset the decoder.
 * Should be the same as calling Dispose and Open after each other
 */
void CDVDVideoCodecA10::Reset()
{
  CLog::Log(LOGDEBUG, "A10: reset requested");
  m_hcedarv->ioctrl(m_hcedarv, CEDARV_COMMAND_RESET, 0);
}

/*
 * returns true if successfull
 * the data is valid until the next Decode call
 */
bool CDVDVideoCodecA10::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
  if (m_picture.iFlags & DVP_FLAG_ALLOCATED)
  {
    *pDvdVideoPicture = m_picture;
    return true;
  }
  return false;
}

void CDVDVideoCodecA10::SetDropState(bool bDrop)
{
}

const char* CDVDVideoCodecA10::GetName()
{
  return "A10";
}

void CDVDVideoCodecA10::FreePicture(void *pictpriv, cedarv_picture_t &pict)
{
  m_hcedarv->display_release(m_hcedarv, pict.id);
}
