gst_plugins_rtp_sources = [
  'plugin.c',
  'gstrtpsink.c',
  'gstrtpsrc.c',
  'gstrtp-utils.c',
]

gst_plugins_rtp_headers = [
  'gstrtpsink.h',
  'gstrtpsrc.h',
  'gstrtp-utils.h',
]

gstrtp = library('gstnrtp',
  gst_plugins_rtp_sources,
  dependencies: [gio_dep, gst_dep, gstbase_dep, gstrtp_dep, gstnet_dep, gstcontroller_dep],
  include_directories: [configinc],
  install: true,
  c_args: gst_plugins_rtp_args,
  install_dir: plugins_install_dir,
)

# pkgconfig.generate(gstrtp, install_dir: plugins_pkgconfig_install_dir)
