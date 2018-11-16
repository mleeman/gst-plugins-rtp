gst_plugins_rtp_sources = [
  'plugin.c',
  'gstrtpsink.c',
  'gstrtpsrc.c',
]

gst_plugins_rtp_headers = [
  'gstrtpsink.h',
  'gstrtpsrc.h'
]

gstrtp = library('gstrtp',
  gst_plugins_rtp_sources,
  dependencies: gst_plugins_rtp_dependencies,
  include_directories: [configinc],
  install: true,
  c_args: gst_plugins_rtp_args,
  install_dir: plugins_install_dir,
)

pkgconfig.generate(gstrtp, install_dir: plugins_pkgconfig_install_dir)
