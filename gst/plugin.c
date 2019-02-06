#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstrtpsink.h"
#include "gstrtpsrc.h"


static gboolean
plugin_init (GstPlugin * plugin)
{

  gboolean ret = FALSE;

  ret |= gst_element_register (plugin, "rtp_rtpsrc",
      GST_RANK_PRIMARY + 1, GST_TYPE_RTP_SRC);

  ret |= gst_element_register (plugin, "rtp_rtpsink",
      GST_RANK_PRIMARY + 1, GST_TYPE_RTP_SINK);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    nrtp,
    "GStreamer RTP Plugins",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
