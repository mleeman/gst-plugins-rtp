#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstrtpsrc.h"

GST_DEBUG_CATEGORY_STATIC (rtp_src_debug);
#define GST_CAT_DEFAULT rtp_src_debug

#define DEFAULT_PROP_URI              "rtp://0.0.0.0:5004"

struct _GstRtpSrc
{
  GstBin parent_instance;

  /* Properties */
  GstUri *uri;

  GMutex lock;
};

enum
{
  PROP_0,

  PROP_URI,

  PROP_LAST
};

static void gst_rtp_src_uri_handler_init (gpointer g_iface, gpointer iface_data); 

#define gst_rtp_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSrc, gst_rtp_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_rtp_src_uri_handler_init));

static void
gst_rtp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSrc *self = GST_RTP_SRC (object);

  switch (prop_id) {
    case PROP_URI:
      if (self->uri) gst_uri_unref (self->uri);
      self->uri = gst_uri_from_string (g_value_get_string (value));
      break;
        default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtpSrc *self = GST_RTP_SRC (object);

  switch (prop_id) {
    case PROP_URI:
      if (self->uri)
        g_value_take_string (value, gst_uri_to_string (self->uri));
      else
        g_value_set_string (value, NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_src_finalize (GObject * gobject)
{
  GstRtpSrc *self = GST_RTP_SRC (gobject);

  if (self->uri) gst_uri_unref (self->uri);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_rtp_src_class_init (GstRtpSrcClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  oclass->set_property = gst_rtp_src_set_property;
  oclass->get_property = gst_rtp_src_get_property;
  oclass->finalize = gst_rtp_src_finalize;

  g_object_class_install_property (oclass, PROP_URI,
      g_param_spec_string ("uri", "URI", "URI to send data on",
          DEFAULT_PROP_URI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (rtp_src_debug,
      "rtpsrc", 0, "GStreamer RTP src");
}

static void
gst_rtp_src_init (GstRtpSrc * self)
{
  self->uri = gst_uri_from_string (DEFAULT_PROP_URI);

  GST_OBJECT_FLAG_SET (GST_OBJECT (self), GST_ELEMENT_FLAG_SRC);
}

static guint
gst_rtp_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_rtp_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { (char *) "rtp", NULL };

  return protocols;
}

static gchar *
gst_rtp_src_uri_get_uri (GstURIHandler * handler)
{
  GstRtpSrc *self = (GstRtpSrc *) handler;

  return gst_uri_to_string (self->uri);
}

static gboolean
gst_rtp_src_uri_set_uri (GstURIHandler * handler, const gchar * uri, GError ** error)
{
  GstRtpSrc *self = (GstRtpSrc *) handler;

  g_object_set (G_OBJECT (self), "uri", uri, NULL);

  return TRUE;
}

static void
gst_rtp_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rtp_src_uri_get_type;
  iface->get_protocols = gst_rtp_src_uri_get_protocols;
  iface->get_uri = gst_rtp_src_uri_get_uri;
  iface->set_uri = gst_rtp_src_uri_set_uri;
}


