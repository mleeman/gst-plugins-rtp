gst_plugins_rtp_sources = [
  'plugin.c',
  'gstrtpsink.c',
  'gstrtpsrc.c',
  'gst_object_set_properties_from_uri_query.c',
]

gst_plugins_rtp_headers = [
  'gstrtpsink.h',
  'gstrtpsrc.h',
  'gst_object_set_properties_from_uri_query.h',
]

gstrtp = library('gstnrtp',
  gst_plugins_rtp_sources,
  dependencies: [gst_dep, gstbase_dep, gstrtp_dep, gstnet_dep, gstcontroller_dep],
  include_directories: [configinc],
  install: true,
  c_args: gst_plugins_rtp_args,
  install_dir: plugins_install_dir,
)

# pkgconfig.generate(gstrtp, install_dir: plugins_pkgconfig_install_dir)
