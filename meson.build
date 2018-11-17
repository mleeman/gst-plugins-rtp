project('gst-plugins-rtp', 'c', 
  version : '1.15.0.1',
  meson_version : '>= 0.46.0',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

gst_version = meson.project_version()
version_arr = gst_version.split('.')
gst_version_major = version_arr[0].to_int()
gst_version_minor = version_arr[1].to_int()
gst_version_micro = version_arr[2].to_int()
 if version_arr.length() == 4
  gst_version_nano = version_arr[3].to_int()
else
  gst_version_nano = 0
endif

gst_version_is_dev = gst_version_minor % 2 == 1 and gst_version_micro < 90
glib_req = '>= 2.40.0'
gst_req = '>= @0@.@1@.0'.format(gst_version_major, gst_version_minor)

cc = meson.get_compiler('c')
cdata = configuration_data()

check_headers = [
] 
foreach h : check_headers   
  if cc.has_header(h.get(1))     
    cdata.set(h.get(0), 1)   
  endif 
endforeach

libm = cc.find_library('m', required : false)

warning_flags = [   
  '-Wmissing-declarations',   
  '-Wredundant-decls',   
  '-Wundef',   
  '-Wwrite-strings',   
  '-Wformat',   
  '-Wformat-nonliteral',   
  '-Wformat-security',   
  '-Winit-self',   
  '-Wmissing-include-dirs',   
  '-Waddress',   
  '-Wno-multichar',   
  '-Wvla',   
  '-Wpointer-arith', 
]

warning_c_flags = [   
  '-Wmissing-prototypes',   
  '-Wdeclaration-after-statement', 
]

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

foreach extra_arg : warning_c_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

# GStreamer package name and origin url
gst_package_name = get_option('package-name')
if gst_package_name == ''
  if gst_version_nano == 0
    gst_package_name = 'GStreamer Base Plug-ins source release'
  elif gst_version_nano == 1
    gst_package_name = 'GStreamer Base Plug-ins git'
  else
    gst_package_name = 'GStreamer Base Plug-ins prerelease'
  endif
endif
cdata.set_quoted('GST_PACKAGE_NAME', gst_package_name)
cdata.set_quoted('GST_PACKAGE_ORIGIN', get_option('package-origin'))

# Mandatory GST deps
gst_dep = dependency('gstreamer-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_base_dep'])
gstnet_dep = dependency('gstreamer-net-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_net_dep'])
gstcontroller_dep = dependency('gstreamer-controller-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_controller_dep'])

configure_file(output : 'config.h', configuration : cdata)

subdir('gst')
