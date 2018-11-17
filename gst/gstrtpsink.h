#ifndef _GST_RTP_SINK_H_
#define _GST_RTP_SINK_H_

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_RTP_SINK (gst_rtp_sink_get_type ())
G_DECLARE_FINAL_TYPE (GstRtpSink, gst_rtp_sink, GST, RTP_SINK, GstBin);
G_END_DECLS

#endif
