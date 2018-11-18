#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstrtpsink.h"

GST_DEBUG_CATEGORY_STATIC (rtp_sink_debug);
#define GST_CAT_DEFAULT rtp_sink_debug

struct _GstRtpSink
{
  GstBin parent_instance;
  GMutex lock;
};

enum
{
  PROP_0,
  PROP_LAST
};

#define gst_rtp_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSink, gst_rtp_sink, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_rtp_sink_uri_handler_init));

static void
gst_rtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSink *self = GST_RTP_SINK (object);
  switch (prop_id) {
        default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtpSink *self = GST_RTP_SINK (object);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_sink_finalize (GObject * gobject)
{
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_rtp_sink_class_init (GstRtpSinkClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  oclass->set_property = gst_rtp_sink_set_property;
  oclass->get_property = gst_rtp_sink_get_property;
  oclass->finalize = gst_rtp_sink_finalize;

  GST_DEBUG_CATEGORY_INIT (rtp_sink_debug,
      "rtpsink", 0, "GStreamer RTP sink");
}

static void
gst_rtp_sink_init (GstRtpSink * self)
{

}
