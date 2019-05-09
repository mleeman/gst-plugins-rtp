/* GStreamer
 * Copyright (C) <2018> Marc Leeman <marc.leeman@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION: gstrtpsrc
 * @title: GstRtpSrc
 * @short description: element with Uri interface to get RTP data from
 * the network.
 *
 * RTP (RFC 3550) is a protocol to stream media over the network while
 * retaining the timing information and providing enough information to
 * reconstruct the correct timing domain by the receiver.
 *
 * The RTP data port should be even, while the RTCP port should be
 * odd. The URI that is entered defines the data port, the RTCP port will
 * be allocated to the next port.
 *
 * This element hooks up the correct sockets to support both RTP as the
 * accompanying RTCP layer.
 *
 * This Bin handles taking in of data from the network and provides the
 * RTP payloaded data.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <gst/rtp/gstrtppayloads.h>

#include "gstrtpsrc.h"
#include "gstrtp-utils.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtp_src_debug);
#define GST_CAT_DEFAULT gst_rtp_src_debug

#define DEFAULT_PROP_TTL              64
#define DEFAULT_PROP_TTL_MC           1
#define DEFAULT_PROP_ENCODING_NAME    NULL
#define DEFAULT_PROP_LATENCY          200

#define DEFAULT_PROP_URI              "rtp://0.0.0.0:5004"

struct _GstRtpSrc
{
  GstBin parent_instance;

  /* Properties */
  GstUri *uri;
  gint ttl;
  gint ttl_mc;
  gint latency;
  gchar *encoding_name;
  guint latency_ms;

  /* Internal elements */
  GstElement *rtpbin;
  GstElement *udpsrc_rtp;
  GstElement *udpsrc_rtcp;
  GstElement *udpsink_rtcp;

  GMutex lock;
};

enum
{
  PROP_0,

  PROP_URI,
  PROP_TTL,
  PROP_TTL_MC,
  PROP_ENCODING_NAME,
  PROP_LATENCY,

  PROP_LAST
};

static void gst_rtp_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

#define gst_rtp_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSrc, gst_rtp_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_rtp_src_uri_handler_init);
    GST_DEBUG_CATEGORY_INIT (gst_rtp_src_debug, "nrtp_rtpsrc", 0, "RTP Source"));

#define GST_RTP_SRC_GET_LOCK(obj) (&((GstRtpSrc*)(obj))->lock)
#define GST_RTP_SRC_LOCK(obj) (g_mutex_lock (GST_RTP_SRC_GET_LOCK(obj)))
#define GST_RTP_SRC_UNLOCK(obj) (g_mutex_unlock (GST_RTP_SRC_GET_LOCK(obj)))

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStateChangeReturn
gst_rtp_src_change_state (GstElement * element, GstStateChange transition);

/**
 * gst_rtp_src_rtpbin_erquest_pt_map_cb:
 * @self: The current #GstRtpSrc object
 *
 * #GstRtpBin callback to map a pt on RTP caps.
 *
 * Returns: (transfer none): the guess on the RTP caps based on the PT
 * and caps.
 */
static GstCaps *
gst_rtp_src_rtpbin_request_pt_map_cb (GstElement * rtpbin, guint session_id,
    guint pt, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GstCaps *ret = NULL;
  const GstRTPPayloadInfo *p;

  GST_DEBUG_OBJECT (self,
      "Requesting caps for session-id 0x%x and pt %u.", session_id, pt);

  /* the encoding-name has more relevant information */
  if (self->encoding_name != NULL)
    goto dynamic;

  if (!GST_RTP_PAYLOAD_IS_DYNAMIC (pt)) {
    p = gst_rtp_payload_info_for_pt (pt);
    if (p != NULL)
      return NULL;
  }

  GST_DEBUG_OBJECT (self, "Could not determine caps based on pt and"
      " the encoding-name was not set. Assuming H.264");
  self->encoding_name = g_strdup ("H264");

dynamic:
  /* Unfortunately, the media needs to be passed in the function. Since
   * it is not known, try for video if video not found. */
  p = gst_rtp_payload_info_for_name ("video", self->encoding_name);
  if (p == NULL)
    p = gst_rtp_payload_info_for_name ("audio", self->encoding_name);

  if (p != NULL) {
    ret = gst_caps_new_simple ("application/x-rtp",
        "encoding-name", G_TYPE_STRING, p->encoding_name,
        "clock-rate", G_TYPE_INT, p->clock_rate,
        "media", G_TYPE_STRING, p->media, NULL);

    GST_DEBUG_OBJECT (self, "Decided on caps %" GST_PTR_FORMAT, ret);

    return ret;
  }

  return NULL;
}

static void
gst_rtp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSrc *self = GST_RTP_SRC (object);
  GstCaps *caps;

  switch (prop_id) {
    case PROP_URI:
      if (self->uri)
        gst_uri_unref (self->uri);
      self->uri = gst_uri_from_string (g_value_get_string (value));
      gst_rtp_utils_set_properties_from_uri_query (G_OBJECT (self), self->uri);
      break;
    case PROP_TTL:
      self->ttl = g_value_get_int (value);
      break;
    case PROP_TTL_MC:
      self->ttl_mc = g_value_get_int (value);
      break;
    case PROP_ENCODING_NAME:
      if (self->encoding_name)
        g_free (self->encoding_name);
      self->encoding_name = g_value_dup_string (value);
      if (self->udpsrc_rtp) {
        caps = gst_rtp_src_rtpbin_request_pt_map_cb (NULL, 0, 96, self);
        g_object_set (G_OBJECT (self->udpsrc_rtp), "caps", caps, NULL);
        gst_caps_unref (caps);
      }
      break;
    case PROP_LATENCY:
      self->latency = g_value_get_uint (value);
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
    case PROP_ENCODING_NAME:
      g_value_set_string (value, self->encoding_name);
      break;
    case PROP_LATENCY:
      g_value_set_uint (value, self->latency);
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

  if (self->uri)
    gst_uri_unref (self->uri);

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

  /**
   * GstRtpSrc:uri:
   *
   * uri to an RTP from. All GStreamer parameters can be
   * encoded in the URI, this URI format is RFC compliant.
   *
   * Since: 1.14.4.1
   */
  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI", "URI to send data on",
          DEFAULT_PROP_URI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc:ttl:
   *
   * Set the unicast TTL parameter. In RTP this of importance for RTCP.
   *
   * Since: 1.14.4.1
   */
  g_object_class_install_property (gobject_class, PROP_TTL,
      g_param_spec_int ("ttl", "Unicast TTL",
          "Used for setting the unicast TTL parameter",
          0, 255, DEFAULT_PROP_TTL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc:ttl-mc:
   *
   * Set the multicast TTL parameter. In RTP this of importance for RTCP.
   *
   * Since: 1.14.4.1
   */
  g_object_class_install_property (gobject_class, PROP_TTL_MC,
      g_param_spec_int ("ttl-mc", "Multicast TTL",
          "Used for setting the multicast TTL parameter", 0, 255,
          DEFAULT_PROP_TTL_MC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc:encoding-name:
   *
   * Set the encoding name of the stream to use. This is a short-hand for
   * the full caps and maps typically to the encoding-name in the RTP caps.
   *
   * Since: 1.14.4.1
   */
  g_object_class_install_property (gobject_class, PROP_ENCODING_NAME,
      g_param_spec_string ("encoding-name", "Caps encoding name",
          "Encoding name use to determine caps parameters",
          DEFAULT_PROP_ENCODING_NAME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc:latency:
   *
   * Set the size of the latency buffer in the
   * GstRtpBin/GstRtpJitterBuffer to compensate for network jitter.
   *
   * Since: 1.14.4.1
   */
  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_uint ("latency", "Buffer latency in ms",
          "Default amount of ms to buffer in the jitterbuffers", 0,
          G_MAXUINT, DEFAULT_PROP_LATENCY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "RTP Source element",
      "Generic/Bin/Src",
      "Simple RTP src", "Marc Leeman <marc.leeman@gmail.com>");
}

static void
gst_rtp_src_rtpbin_element_added_cb (GstBin * element, GstElement * new_element,
    gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GST_INFO_OBJECT (self,
      "Element %" GST_PTR_FORMAT " added element %" GST_PTR_FORMAT ".", element,
      new_element);
}

static void
gst_rtp_src_rtpbin_pad_added_cb (GstElement * element, GstPad * pad,
    gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GstCaps *caps = gst_pad_query_caps (pad, NULL);
  GstPad *upad;
  gchar *name;

  /* Expose RTP data pad only */
  GST_INFO_OBJECT (self,
      "Element %" GST_PTR_FORMAT " added pad %" GST_PTR_FORMAT "with caps %"
      GST_PTR_FORMAT ".", element, pad, caps);

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
  name = g_strdup_printf ("src_%u", GST_ELEMENT (self)->numpads);
  upad = gst_ghost_pad_new (name, pad);
  g_free (name);

  gst_pad_set_active (upad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), upad);

  GST_RTP_SRC_UNLOCK (self);
}

static void
gst_rtp_src_rtpbin_pad_removed_cb (GstElement * element, GstPad * pad,
    gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GST_INFO_OBJECT (self,
      "Element %" GST_PTR_FORMAT " removed pad %" GST_PTR_FORMAT ".", element,
      pad);
}

static void
gst_rtp_src_rtpbin_on_ssrc_collision_cb (GstElement * rtpbin, guint session_id,
    guint ssrc, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);

  GST_WARNING_OBJECT (self,
      "Dectected an SSRC collision: session-id 0x%x, ssrc 0x%x.", session_id,
      ssrc);
}

static void
gst_rtp_src_rtpbin_on_new_ssrc_cb (GstElement * rtpbin, guint session_id,
    guint ssrc, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);

  GST_INFO_OBJECT (self, "Dectected a new SSRC: session-id 0x%x, ssrc 0x%x.",
      session_id, ssrc);
}

static gboolean
gst_rtp_src_setup_elements (GstRtpSrc * self)
{
  /*GstPad *pad; */
  GSocket *socket;
  GInetAddress *addr;
  gchar *name;
  GstCaps *caps;

  /* Should not be NULL */
  g_return_val_if_fail (self->uri != NULL, FALSE);

  self->rtpbin = gst_element_factory_make ("rtpbin", NULL);
  if (self->rtpbin == NULL) {
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "rtpbin element is not available"));
    return FALSE;
  }

  self->udpsrc_rtp = gst_element_factory_make ("udpsrc", NULL);
  if (self->udpsrc_rtp == NULL) {
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "udpsrc_rtp element is not available"));
    return FALSE;
  }

  self->udpsrc_rtcp = gst_element_factory_make ("udpsrc", NULL);
  if (self->udpsrc_rtcp == NULL) {
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "udpsrc_rtcp element is not available"));
    return FALSE;
  }

  self->udpsink_rtcp = gst_element_factory_make ("udpsink", NULL);
  if (self->udpsink_rtcp == NULL) {
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "udpsink_rtcp element is not available"));
    return FALSE;
  }

  /* Add rtpbin callbacks to monitor the operation of rtpbin */
  g_signal_connect (self->rtpbin, "element-added",
      G_CALLBACK (gst_rtp_src_rtpbin_element_added_cb), self);
  g_signal_connect (self->rtpbin, "pad-added",
      G_CALLBACK (gst_rtp_src_rtpbin_pad_added_cb), self);
  g_signal_connect (self->rtpbin, "pad-removed",
      G_CALLBACK (gst_rtp_src_rtpbin_pad_removed_cb), self);
  g_signal_connect (self->rtpbin, "request-pt-map",
      G_CALLBACK (gst_rtp_src_rtpbin_request_pt_map_cb), self);
  g_signal_connect (self->rtpbin, "on-new-ssrc",
      G_CALLBACK (gst_rtp_src_rtpbin_on_new_ssrc_cb), self);
  g_signal_connect (self->rtpbin, "on-ssrc-collision",
      G_CALLBACK (gst_rtp_src_rtpbin_on_ssrc_collision_cb), self);

  g_object_set (self->rtpbin, "latency", self->latency, NULL);

  /* Add elements as needed, since udpsrc/udpsink for RTCP share a socket,
   * not all at the same moment */
  gst_bin_add (GST_BIN (self), self->rtpbin);
  gst_bin_add (GST_BIN (self), self->udpsrc_rtp);

  g_object_set (self->udpsrc_rtp,
      "address", gst_uri_get_host (self->uri),
      "port", gst_uri_get_port (self->uri), NULL);

  gst_bin_add (GST_BIN (self), self->udpsink_rtcp);

  /* no need to set address if unicast */
  caps = gst_caps_new_empty_simple ("application/x-rtcp");
  g_object_set (self->udpsrc_rtcp,
      "port", gst_uri_get_port (self->uri) + 1,
      "auto-multicast", TRUE, "caps", caps, NULL);
  gst_caps_unref (caps);

  addr = g_inet_address_new_from_string (gst_uri_get_host (self->uri));
  if (g_inet_address_get_is_multicast (addr)) {
    g_object_set (self->udpsrc_rtcp, "address", gst_uri_get_host (self->uri),
        NULL);
  }
  g_object_unref (addr);

  g_object_set (self->udpsink_rtcp, "host", gst_uri_get_host (self->uri), "port", gst_uri_get_port (self->uri) + 1, "ttl", self->ttl, "ttl-mc", self->ttl_mc, "auto-multicast", FALSE,  /* Set false since we're reusing a socket */
      NULL);

  gst_bin_add (GST_BIN (self), self->udpsrc_rtcp);

  g_object_get (G_OBJECT (self->udpsrc_rtcp), "used-socket", &socket, NULL);
  g_object_set (G_OBJECT (self->udpsink_rtcp), "socket", socket, NULL);

  /* pads are all named */
  name = g_strdup_printf ("recv_rtp_sink_%u", GST_ELEMENT (self)->numpads);
  gst_element_link_pads (self->udpsrc_rtp, "src", self->rtpbin, name);
  g_free (name);

  name = g_strdup_printf ("recv_rtcp_sink_%u", GST_ELEMENT (self)->numpads);
  gst_element_link_pads (self->udpsrc_rtcp, "src", self->rtpbin, name);
  g_free (name);

  gst_element_sync_state_with_parent (self->rtpbin);
  gst_element_sync_state_with_parent (self->udpsrc_rtp);
  gst_element_sync_state_with_parent (self->udpsink_rtcp);

  name = g_strdup_printf ("send_rtcp_src_%u", GST_ELEMENT (self)->numpads);
  gst_element_link_pads (self->rtpbin, name, self->udpsink_rtcp, "sink");
  g_free (name);

  gst_element_sync_state_with_parent (self->udpsrc_rtcp);

  return TRUE;
}

static GstStateChangeReturn
gst_rtp_src_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpSrc *self = GST_RTP_SRC (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (self, "Changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (gst_rtp_src_setup_elements (self) == FALSE)
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ret = GST_STATE_CHANGE_NO_PREROLL;
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      ret = GST_STATE_CHANGE_NO_PREROLL;
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
  self->ttl = DEFAULT_PROP_TTL;
  self->ttl_mc = DEFAULT_PROP_TTL_MC;
  self->encoding_name = DEFAULT_PROP_ENCODING_NAME;
  self->latency = DEFAULT_PROP_LATENCY;

  GST_OBJECT_FLAG_SET (GST_OBJECT (self), GST_ELEMENT_FLAG_SOURCE);
  gst_bin_set_suppressed_flags (GST_BIN (self),
      GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK);
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
gst_rtp_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
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

/* ex: set tabstop=2 shiftwidth=2 expandtab: */
