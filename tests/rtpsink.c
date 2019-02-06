#include <gst/check/gstcheck.h>

GST_START_TEST (test_uri_to_properties)
{
  GstElement *rtpsink;

  gint bind_port, ttl, ttl_mc;

  rtpsink = gst_element_factory_make ("rtp_rtpsink", NULL);

  /* Sets properties to non-default values (make sure this stays in sync) */
  g_object_set (rtpsink,
      "uri", "rtp://1.230.1.2?"
      "&ttl=8"
      "&ttl-mc=9", NULL);

  g_object_get (rtpsink,
      "ttl", &ttl,
      "ttl_mc", &ttl_mc,
      NULL);

  /* Make sure these values are in sync with the one from the URI. */
  g_assert_cmpint (ttl, ==, 8);
  g_assert_cmpint (ttl_mc, ==, 9);

  gst_object_unref (rtpsink);
}
GST_END_TEST;

static Suite *
rtpsink_suite (void)
{
  Suite *s = suite_create ("rtpsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_uri_to_properties);

  return s;
}

GST_CHECK_MAIN (rtpsink);
