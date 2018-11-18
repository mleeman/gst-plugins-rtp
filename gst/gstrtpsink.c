#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <gio/gio.h>

#include "gstrtpsink.h"
#include "gst_object_set_properties_from_uri_query.h"

GST_DEBUG_CATEGORY_STATIC (rtp_sink_debug);
#define GST_CAT_DEFAULT rtp_sink_debug

#define DEFAULT_PROP_URI              "rtp://0.0.0.0:5004"
#define DEFAULT_PROP_TTL              64
#define DEFAULT_PROP_TTL_MC           1

struct _GstRtpSink
{
  GstBin parent_instance;

  /* Properties */
  GstUri *uri;
  gint ttl;
  gint ttl_mc;

  /* Internal elements */
  GstElement *rtpbin;
  GstElement *udpsink_rtp;
  GstElement *udpsrc_rtcp;
  GstElement *udpsink_rtcp;

  /* Internal properties */
  guint npads;

  GMutex lock;
};

enum
{
  PROP_0,

  PROP_URI,
  PROP_TTL,
  PROP_TTL_MC,

  PROP_LAST
};

static void gst_rtp_sink_uri_handler_init (gpointer g_iface, gpointer iface_data); 

#define gst_rtp_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSink, gst_rtp_sink, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_rtp_sink_uri_handler_init));

#define GST_RTP_SINK_GET_LOCK(obj) (&((GstRtpSink*)(obj))->lock)
#define GST_RTP_SINK_LOCK(obj) (g_mutex_lock (GST_RTP_SINK_GET_LOCK(obj)))
#define GST_RTP_SINK_UNLOCK(obj) (g_mutex_unlock (GST_RTP_SINK_GET_LOCK(obj)))

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStateChangeReturn
gst_rtp_sink_change_state (GstElement *element, GstStateChange transition);

static void
gst_rtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSink *self = GST_RTP_SINK (object);

  switch (prop_id) {
    case PROP_URI:
      if (self->uri) gst_uri_unref (self->uri);
      self->uri = gst_uri_from_string (g_value_get_string (value));
      gst_object_set_properties_from_uri_query (G_OBJECT (self), self->uri);
      break;
    case PROP_TTL:
      self->ttl = g_value_get_int (value);
      break;
    case PROP_TTL_MC:
      self->ttl_mc = g_value_get_int (value);
      break;
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

  switch (prop_id) {
    case PROP_URI:
      if (self->uri)
        g_value_take_string (value, gst_uri_to_string (self->uri));
      else
        g_value_set_string (value, NULL);
      break;
    case PROP_TTL:
      g_value_set_int (value, self->ttl);
      break;
    case PROP_TTL_MC:
      g_value_set_int (value, self->ttl_mc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_sink_finalize (GObject * gobject)
{
  GstRtpSink *self = GST_RTP_SINK (gobject);

  if (self->uri) gst_uri_unref (self->uri);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static GstPad *
gst_rtp_sink_request_new_pad (GstElement *element,
    GstPadTemplate *templ, const gchar *name, const GstCaps *caps)
{
  GstRtpSink *self = GST_RTP_SINK (element);
  GstPad *pad = NULL;
  gchar *nname = g_strdup_printf ("send_rtp_sink_%u", self->npads);

  g_return_val_if_fail (self->rtpbin != NULL, NULL);
  
  GST_RTP_SINK_LOCK (self);
  pad = gst_element_get_request_pad (self->rtpbin, nname);
  g_return_val_if_fail (pad != NULL, NULL);
  g_free(nname);

  self->npads++;
  GST_RTP_SINK_UNLOCK (self);

  return pad;
}

static void
gst_rtp_sink_release_pad (GstElement *element, GstPad *pad)
{
  GstRtpSink *self = GST_RTP_SINK (element);
  GstPad *rpad = gst_ghost_pad_get_target(GST_GHOST_PAD(pad));

  GST_RTP_SINK_LOCK (self);
  gst_element_release_request_pad (self->rtpbin, rpad);
  gst_object_unref(rpad);

  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT (self), pad);

  self->npads--;
  GST_RTP_SINK_UNLOCK (self);
}
  
static void
gst_rtp_sink_class_init (GstRtpSinkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_rtp_sink_set_property;
  gobject_class->get_property = gst_rtp_sink_get_property;
  gobject_class->finalize = gst_rtp_sink_finalize;
  gstelement_class->change_state = gst_rtp_sink_change_state;

  gstelement_class->request_new_pad = GST_DEBUG_FUNCPTR (gst_rtp_sink_request_new_pad);
  gstelement_class->release_pad = GST_DEBUG_FUNCPTR (gst_rtp_sink_release_pad);

  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI", "URI to send data on",
          DEFAULT_PROP_URI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TTL,
      g_param_spec_int ("ttl", "Unicast TTL",
          "Used for setting the unicast TTL parameter",
          0, 255, DEFAULT_PROP_TTL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TTL_MC,
      g_param_spec_int ("ttl-mc", "Multicast TTL",
          "Used for setting the multicast TTL parameter",
          0, 255, DEFAULT_PROP_TTL_MC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (rtp_sink_debug,
      "rtpsink", 0, "GStreamer RTP sink");
}

static void
gst_rtp_sink_rtpbin_element_added_cb (GstBin *element, GstElement *new_element, gpointer data)
{
  GstRtpSink *self = GST_RTP_SINK (data);
  GST_INFO_OBJECT (self, "Element %" GST_PTR_FORMAT " added element %" GST_PTR_FORMAT ".", element, new_element);
}

static void
gst_rtp_sink_rtpbin_pad_added_cb (GstElement *element, GstPad *pad, gpointer data)
{
  GstRtpSink *self = GST_RTP_SINK (data);
  GstPad *upad;

  GST_INFO_OBJECT (self, "Element %" GST_PTR_FORMAT " added pad %" GST_PTR_FORMAT ".", element, pad);

  /* TODO: funnel? */
  upad = gst_element_get_compatible_pad (self->udpsink_rtp, pad, NULL);
  gst_pad_link (pad, upad);
  gst_object_unref (upad);
}

static void
gst_rtp_sink_rtpbin_pad_removed_cb (GstElement *element, GstPad *pad, gpointer data)
{
  GstRtpSink *self = GST_RTP_SINK (data);
  GST_INFO_OBJECT (self, "Element %" GST_PTR_FORMAT " removed pad %" GST_PTR_FORMAT ".", element, pad);
}

static gboolean
gst_rtp_sink_is_multicast (const gchar * ip_addr)
{
  in_addr_t host;
  struct in6_addr host6;

  /* IPv4 and IPv6 test */
  if ((inet_pton (AF_INET6, ip_addr, &host6) == 1 &&
          IN6_IS_ADDR_MULTICAST (host6.__in6_u.__u6_addr8)) ||
      (inet_pton (AF_INET, ip_addr, &host) == 1 &&
          (host = ntohl (host)) && IN_MULTICAST (host)))
    return TRUE;
  else
    return FALSE;
}

static void
gst_rtp_sink_setup_elements(GstRtpSink *self)
{
  /*GstPad *pad;*/
  GSocket *socket;
  gchar* name;
  GstCaps *caps;

  /* Should not be NULL */
  g_return_if_fail (self->uri != NULL);

  self->rtpbin = gst_element_factory_make("rtpbin", NULL);
  self->udpsink_rtp = gst_element_factory_make("udpsink", NULL);
  self->udpsrc_rtcp = gst_element_factory_make("udpsrc", NULL);
  self->udpsink_rtcp = gst_element_factory_make("udpsink", NULL);

  if (self->rtpbin == NULL)
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "rtpbin element is not available"));

  if (self->udpsink_rtp == NULL)
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "udpsink_rtp element is not available"));

  if (self->udpsrc_rtcp == NULL)
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "udpsrc_rtcp element is not available"));

  if (self->udpsink_rtcp == NULL)
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "udpsink_rtcp element is not available"));

  /* Add rtpbin callbacks to monitor the operation of rtpbin */
  g_signal_connect (self->rtpbin, "element-added",
      G_CALLBACK (gst_rtp_sink_rtpbin_element_added_cb), self);
  g_signal_connect (self->rtpbin, "pad-added",
      G_CALLBACK (gst_rtp_sink_rtpbin_pad_added_cb), self);
  g_signal_connect (self->rtpbin, "pad-removed",
      G_CALLBACK (gst_rtp_sink_rtpbin_pad_removed_cb), self);
  /* Add elements as needed, since udpsrc/udpsink for RTCP share a socket,
   * not all at the same moment */
  gst_bin_add (GST_BIN (self), self->rtpbin);
  gst_bin_add (GST_BIN (self), self->udpsink_rtp);

  g_object_set(self->udpsink_rtp, 
      "host", gst_uri_get_host(self->uri),
      "port", gst_uri_get_port(self->uri),
      "ttl", self->ttl,
      "ttl-mc", self->ttl_mc,
      NULL);

  gst_bin_add (GST_BIN (self), self->udpsink_rtcp);

  /* no need to set address if unicast */
  caps = gst_caps_from_string ("application/x-rtcp");
  g_object_set(self->udpsrc_rtcp, 
      "port", gst_uri_get_port(self->uri) + 1,
      "auto-multicast", TRUE,
      "caps", caps,
      NULL);
  gst_caps_unref(caps);
  if (gst_rtp_sink_is_multicast (gst_uri_get_host(self->uri))) {
    g_object_set(self->udpsrc_rtcp, 
        "address", gst_uri_get_host(self->uri),
        NULL);
  }

  g_object_set(self->udpsink_rtcp, 
      "host", gst_uri_get_host(self->uri),
      "port", gst_uri_get_port(self->uri) + 1,
      "ttl", self->ttl,
      "ttl-mc", self->ttl_mc,
      "auto-multicast", FALSE, /* Set false since we're reusing a socket */
      NULL);

  /* pads are all named */
  name = g_strdup_printf ("send_rtp_src_%u", self->npads);
  gst_element_link_pads (self->rtpbin, name, self->udpsink_rtp, "sink");
  g_free(name);

  name = g_strdup_printf ("send_rtcp_src_%u", self->npads);
  gst_element_link_pads (self->rtpbin, name, self->udpsink_rtcp, "sink");
  g_free(name);

  gst_element_sync_state_with_parent (self->rtpbin);
  gst_element_sync_state_with_parent (self->udpsink_rtp);
  gst_element_sync_state_with_parent (self->udpsink_rtcp);

  gst_bin_add (GST_BIN (self), self->udpsrc_rtcp);

  g_object_get (G_OBJECT (self->udpsrc_rtcp), "used-socket", &socket, NULL);
  g_object_set (G_OBJECT (self->udpsink_rtcp), "socket", socket, NULL);

  name = g_strdup_printf ("send_rtcp_sink_%u", self->npads);
  gst_element_link_pads (self->rtpbin, name, self->udpsrc_rtcp, "src");
  g_free(name);

  gst_element_sync_state_with_parent (self->udpsrc_rtcp);
}

static GstStateChangeReturn
gst_rtp_sink_change_state (GstElement *element, GstStateChange transition)
{
  GstRtpSink *self = GST_RTP_SINK (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG ("changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      gst_rtp_sink_setup_elements(self);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  return ret;
}


static void
gst_rtp_sink_init (GstRtpSink *self)
{
  self->rtpbin = NULL;
  self->udpsink_rtp = NULL;
  self->udpsrc_rtcp = NULL;
  self->udpsink_rtcp = NULL;

  self->uri = gst_uri_from_string (DEFAULT_PROP_URI);
  self->npads = 0u;
  self->ttl = DEFAULT_PROP_TTL;
  self->ttl_mc = DEFAULT_PROP_TTL_MC;

  GST_OBJECT_FLAG_SET (GST_OBJECT (self), GST_ELEMENT_FLAG_SINK);
}

static guint
gst_rtp_sink_uri_get_type (GType type)
{
  return GST_URI_SINK;
}

static const gchar *const *
gst_rtp_sink_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { (char *) "rtp", NULL };

  return protocols;
}

static gchar *
gst_rtp_sink_uri_get_uri (GstURIHandler * handler)
{
  GstRtpSink *self = (GstRtpSink *) handler;

  return gst_uri_to_string (self->uri);
}

static gboolean
gst_rtp_sink_uri_set_uri (GstURIHandler * handler, const gchar * uri, GError ** error)
{
  GstRtpSink *self = (GstRtpSink *) handler;

  g_object_set (G_OBJECT (self), "uri", uri, NULL);

  return TRUE;
}

static void
gst_rtp_sink_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rtp_sink_uri_get_type;
  iface->get_protocols = gst_rtp_sink_uri_get_protocols;
  iface->get_uri = gst_rtp_sink_uri_get_uri;
  iface->set_uri = gst_rtp_sink_uri_set_uri;
}


