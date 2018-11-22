#ifndef __GSTRTPCAPS_H__
#define __GSTRTPCAPS_H__

#include <glib.h>
#include <stdlib.h>
#include <string.h>

G_BEGIN_DECLS typedef struct _RtpCaps RtpCaps;

struct _RtpCaps
{
  gint pt;
  const gchar *encoding_name;
  const gchar *media;
  gint clock_rate;
};

const static RtpCaps RTP_STATIC_CAPS[] = {
  {0, "PCMU", "audio", 8000},
  {3, "GSM", "audio", 8000},
  {4, "G723", "audio", 8000},
  {5, "DVI4", "audio", 8000},
  {6, "DVI4", "audio", 16000},
  {7, "LPC", "audio", 8000},
  {8, "PCMA", "audio", 8000},
  {9, "G722", "audio", 8000},
  {10, "L16", "audio", 48000},
  {11, "L16", "audio", 48000},
  {12, "QCELP", "audio", 8000},
  {13, "CN", "audio", 8000},
  {14, "MPA", "audio", 90000},
  {15, "G728", "audio", 8000},
  {16, "DVI4", "audio", 11025},
  {17, "DVI4", "audio", 22050},
  {18, "G729", "audio", 8000},
  {25, "CelB", "video", 90000},
  {26, "JPEG", "video", 90000},
  {28, "nv", "video", 90000},
  {31, "H261", "video", 90000},
  {32, "MPV", "video", 90000},
  {33, "MP2T", "video", 90000},
  {34, "H263", "video", 90000},
  {-1, NULL, NULL, 0}
};

const static RtpCaps RTP_DYNAMIC_CAPS[] = {
  {0, "MP4V-ES", "video", 90000},
  {0, "H264", "video", 90000},
  {0, "H265", "video", 90000},
  {0, "MP2P", "video", 90000},
  {0, "H263-1998", "video", 90000},
  {0, "H263-2000", "video", 90000},
  {0, "MP1S", "video", 90000},
  {0, "AMR", "audio", 8000},
  {0, "AMR-WB", "audio", 16000},
  {0, "DAT12", "audio", 0},
  {0, "dsr-es201108", "audio", 0},
  {0, "EVRC", "audio", 8000},
  {0, "EVRC0", "audio", 8000},
  {0, "EVRC1", "audio", 8000},
  {0, "EVRCB", "audio", 8000},
  {0, "EVRCB0", "audio", 8000},
  {0, "EVRCB1", "audio", 8000},
  {0, "EVRCWB", "audio", 0},
  {0, "EVRCWB0", "audio", 0},
  {0, "EVRCWB1", "audio", 0},
  {0, "G7221", "audio", 16000},
  {0, "G726-16", "audio", 8000},
  {0, "G726-24", "audio", 8000},
  {0, "G726-32", "audio", 8000},
  {0, "G726-40", "audio", 8000},
  {0, "G729D", "audio", 8000},
  {0, "G729E", "audio", 8000},
  {0, "GSM-EFR", "audio", 8000},
  {0, "L8", "audio", 0},
  {0, "RED", "audio", 0},
  {0, "rtx", "audio", 0},
  {0, "VDVI", "audio", 0},
  {0, "L20", "audio", 0},
  {0, "L24", "audio", 0},
  {0, "MP4A-LATM", "audio", 48000},
  {0, "mpa-robust", "audio", 90000},
  {0, "parityfec", "audio", 0},
  {0, "SMV", "audio", 8000},
  {0, "SMV0", "audio", 8000},
  {0, "t140c", "audio", 0},
  {0, "t38", "audio", 0},
  {0, "telephone-event", "audio", 0},
  {0, "tone", "audio", 0},
  {0, "DVI4", "audio", 0},
  {0, "G722", "audio", 0},
  {0, "G723", "audio", 0},
  {0, "G728", "audio", 0},
  {0, "G729", "audio", 0},
  {0, "GSM", "audio", 0},
  {0, "L16", "audio", 48000},
  {0, "LPC", "audio", 0},
  {0, "PCMA", "audio", 0},
  {0, "PCMU", "audio", 0},
  {0, "OPUS", "audio", 48000},
  {0, "BMPEG", "video", 90000},
  {0, "BT656", "video", 90000},
  {0, "DV", "video", 90000},
  {0, "parityfec", "video", 0},
  {0, "pointer", "video", 90000},
  {0, "raw", "video", 90000},
  {0, "rtx", "video", 0},
  {0, "SMPTE292M", "video", 0},
  {0, "vc1", "video", 90000},
  {0, "THEORA", "video", 90000},
  {0, "VP8", "video", 90000},
  {0, "VP8-DRAFT-IETF-01", "video", 90000},
  {0, "VP9", "video", 90000},
  {0, "VP9-DRAFT-IETF-01", "video", 90000},
  {0, "X-GST", "media", 9000},
  {-1, NULL, NULL, 0}
};

G_END_DECLS
#endif
