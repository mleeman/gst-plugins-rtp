#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <gio/gio.h>

#include "gstrtpsrc.h"
#include "gstrtpcaps.h"
#include "gst_object_set_properties_from_uri_query.h"

GST_DEBUG_CATEGORY_STATIC (rtp_src_debug);
#define GST_CAT_DEFAULT rtp_src_debug
#define DEFAULT_PROP_TTL              64
#define DEFAULT_PROP_TTL_MC           1

#define DEFAULT_PROP_URI              "rtp://0.0.0.0:5004"

struct _GstRtpSrc
{
  GstBin parent_instance;

  /* Properties */
  GstUri *uri;
  gint ttl;
  gint ttl_mc;
  gint latency;

  /* Internal elements */
  GstElement *rtpbin;
  GstElement *udpsrc_rtp;
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

static void gst_rtp_src_uri_handler_init (gpointer g_iface, gpointer iface_data);

#define gst_rtp_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSrc, gst_rtp_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_rtp_src_uri_handler_init));

#define GST_RTP_SRC_GET_LOCK(obj) (&((GstRtpSrc*)(obj))->lock)
#define GST_RTP_SRC_LOCK(obj) (g_mutex_lock (GST_RTP_SRC_GET_LOCK(obj)))
#define GST_RTP_SRC_UNLOCK(obj) (g_mutex_unlock (GST_RTP_SRC_GET_LOCK(obj)))

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStateChangeReturn
gst_rtp_src_change_state (GstElement *element, GstStateChange transition);

static void
gst_rtp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSrc *self = GST_RTP_SRC (object);

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
gst_rtp_src_finalize (GObject * gobject)
{
  GstRtpSrc *self = GST_RTP_SRC (gobject);

  if (self->uri) gst_uri_unref (self->uri);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_rtp_src_class_init (GstRtpSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_rtp_src_set_property;
  gobject_class->get_property = gst_rtp_src_get_property;
  gobject_class->finalize = gst_rtp_src_finalize;
  gstelement_class->change_state = gst_rtp_src_change_state;

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

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  GST_DEBUG_CATEGORY_INIT (rtp_src_debug,
      "rtpsrc", 0, "GStreamer RTP src");

  gst_element_class_set_static_metadata (gstelement_class,
      "rtpsrc",
      "Generic/Bin/Src",
      "Simple Rtp src",
      "Marc Leeman <marc.leeman@gmail.com>");
}

static void
gst_rtp_src_rtpbin_element_added_cb (GstBin *element, GstElement *new_element, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GST_INFO_OBJECT (self, "Element %" GST_PTR_FORMAT " added element %" GST_PTR_FORMAT ".", element, new_element);
}

static void
gst_rtp_src_rtpbin_pad_added_cb (GstElement *element, GstPad *pad, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GstCaps* caps = gst_pad_query_caps (pad, NULL);
  GstPad *upad;
  gchar *name;

  /* Expose RTP data pad only */
  GST_INFO_OBJECT (self, "Element %" GST_PTR_FORMAT " added pad %" GST_PTR_FORMAT "with caps %" GST_PTR_FORMAT ".", element, pad, caps);

  /* Sanity checks */
  if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK) {
    /* Sink pad, do not expose */
    gst_caps_unref (caps);
    return;
  }

  if (G_LIKELY (caps)) {
    GstCaps *ref_caps = gst_caps_new_empty_simple ("application/x-rtcp");

    if (gst_caps_can_intersect (caps, ref_caps)) {
      /* SRC RTCP caps, do not expose */
      gst_caps_unref (ref_caps);
      gst_caps_unref (caps);

      return;
    }
    gst_caps_unref (ref_caps);
  } else {
    GST_ERROR_OBJECT (self, "Pad with no caps detected.");
    gst_caps_unref (caps);

    return;
  }
  gst_caps_unref (caps);

  GST_RTP_SRC_LOCK (self);
  /*name = g_strdup_printf("src_%u", self->npads++);*/
  name = g_strdup_printf("src_%u", 0);
  upad = gst_ghost_pad_new (name, pad);
  g_free(name);

  gst_pad_set_active(upad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), upad);

  GST_RTP_SRC_UNLOCK (self);
}

static gboolean
gst_rtp_src_is_multicast (const gchar * ip_addr)
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
gst_rtp_src_rtpbin_pad_removed_cb (GstElement *element, GstPad *pad, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GST_INFO_OBJECT (self, "Element %" GST_PTR_FORMAT " removed pad %" GST_PTR_FORMAT ".", element, pad);
}

static GstCaps *
gst_rtp_src_rtpbin_request_pt_map_cb (GstElement *session, guint ssrc, guint pt, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GstCaps *ret = NULL;
  const RtpCaps *p;
  gchar *encoding_name = NULL;
  int i = 0;

  GST_DEBUG_OBJECT (self, "Requesting caps for session %u and pt %u in session %u.", ssrc, pt);

  i = 0;
  while (RTP_STATIC_CAPS[i].pt >= 0) {
    p = &(RTP_STATIC_CAPS[i++]);
    if (p->pt == pt) {
      goto beach;
    }
  }

  encoding_name = g_strdup ("H264");

dynamic:
  i = 0;
  while (RTP_DYNAMIC_CAPS[i].pt >= 0) {
    p = &(RTP_DYNAMIC_CAPS[i++]);
    if (g_strcmp0 (p->encoding_name, encoding_name) == 0) {
      goto beach;
    }
  }

  i = 0;
  /* lookup the caps based on encoding-name */
  while (RTP_STATIC_CAPS[i].pt >= 0) {
    p = &(RTP_STATIC_CAPS[i++]);
    if (g_strcmp0 (p->encoding_name, encoding_name) == 0) {
      goto beach;
    }
  }

  return NULL;

beach:

  ret = gst_caps_new_simple ("application/x-rtp",
      "encoding-name", G_TYPE_STRING, p->encoding_name,
      "clock-rate", G_TYPE_INT, p->clock_rate,
      "media", G_TYPE_STRING, p->media,
      NULL);

  return ret;
}

static void
gst_rtp_src_setup_elements(GstRtpSrc *self)
{
  /*GstPad *pad;*/
  GSocket *socket;
  gchar* name;
  GstCaps *caps;

  /* Should not be NULL */
  g_return_if_fail (self->uri != NULL);

  self->rtpbin = gst_element_factory_make("rtpbin", NULL);
  self->udpsrc_rtp = gst_element_factory_make("udpsrc", NULL);
  self->udpsrc_rtcp = gst_element_factory_make("udpsrc", NULL);
  self->udpsink_rtcp = gst_element_factory_make("udpsink", NULL);

  if (self->rtpbin == NULL)
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "rtpbin element is not available"));

  if (self->udpsrc_rtp == NULL)
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "udpsrc_rtp element is not available"));

  if (self->udpsrc_rtcp == NULL)
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "udpsrc_rtcp element is not available"));

  if (self->udpsink_rtcp == NULL)
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "udpsink_rtcp element is not available"));

  /* Add rtpbin callbacks to monitor the operation of rtpbin */
  g_signal_connect (self->rtpbin, "element-added",
      G_CALLBACK (gst_rtp_src_rtpbin_element_added_cb), self);
  g_signal_connect (self->rtpbin, "pad-added",
      G_CALLBACK (gst_rtp_src_rtpbin_pad_added_cb), self);
  g_signal_connect (self->rtpbin, "pad-removed",
      G_CALLBACK (gst_rtp_src_rtpbin_pad_removed_cb), self);
  g_signal_connect (self->rtpbin, "request-pt-map",
      G_CALLBACK (gst_rtp_src_rtpbin_request_pt_map_cb), self);
#if 0
  g_signal_connect (self->rtpbin, "on-new-ssrc",
      G_CALLBACK (gst_rtp_src_rtpbin_on_new_ssrc_cb), self);
  g_signal_connect (self->rtpbin, "on-ssrc-collision",
      G_CALLBACK (gst_rtp_src_rtpbin_on_ssrc_collision_cb), self);
#endif

  /* Add elements as needed, since udpsrc/udpsink for RTCP share a socket,
   * not all at the same moment */
  gst_bin_add (GST_BIN (self), self->rtpbin);
  gst_bin_add (GST_BIN (self), self->udpsrc_rtp);

  g_object_set(self->udpsrc_rtp,
      "address", gst_uri_get_host(self->uri),
      "port", gst_uri_get_port(self->uri),
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
  if (gst_rtp_src_is_multicast (gst_uri_get_host(self->uri))) {
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

  gst_bin_add (GST_BIN (self), self->udpsrc_rtcp);

  g_object_get (G_OBJECT (self->udpsrc_rtcp), "used-socket", &socket, NULL);
  g_object_set (G_OBJECT (self->udpsink_rtcp), "socket", socket, NULL);

  /* pads are all named */
  name = g_strdup_printf ("recv_rtp_sink_%u", self->npads);
  gst_element_link_pads (self->udpsrc_rtp, "src", self->rtpbin, name);
  g_free(name);

  name = g_strdup_printf ("recv_rtcp_sink_%u", self->npads);
  gst_element_link_pads (self->udpsrc_rtcp, "src", self->rtpbin, name);
  g_free(name);

  gst_element_sync_state_with_parent (self->rtpbin);
  gst_element_sync_state_with_parent (self->udpsrc_rtp);
  gst_element_sync_state_with_parent (self->udpsink_rtcp);

  name = g_strdup_printf ("send_rtcp_src_%u", self->npads);
  gst_element_link_pads (self->rtpbin, name, self->udpsink_rtcp, "sink");
  g_free(name);

  gst_element_sync_state_with_parent (self->udpsrc_rtcp);
}

static GstStateChangeReturn
gst_rtp_src_change_state (GstElement *element, GstStateChange transition)
{
  GstRtpSrc *self = GST_RTP_SRC (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (self, "Changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      gst_rtp_src_setup_elements(self);
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
gst_rtp_src_init (GstRtpSrc * self)
{
  self->rtpbin = NULL;
  self->udpsrc_rtp = NULL;
  self->udpsrc_rtcp = NULL;
  self->udpsink_rtcp = NULL;

  self->uri = gst_uri_from_string (DEFAULT_PROP_URI);
  self->npads = 0u;
  self->ttl = DEFAULT_PROP_TTL;
  self->ttl_mc = DEFAULT_PROP_TTL_MC;

  GST_OBJECT_FLAG_SET (GST_OBJECT (self), GST_ELEMENT_FLAG_SOURCE);
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


