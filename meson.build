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

cc = meson.get_compiler('c')

check_headers = [
] 
foreach h : check_headers   
  if cc.has_header(h.get(1))     
    core_conf.set(h.get(0), 1)   
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
  if have_cxx and cxx.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'cpp')
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
core_conf.set_quoted('GST_PACKAGE_NAME', gst_package_name)
core_conf.set_quoted('GST_PACKAGE_ORIGIN', get_option('package-origin'))

# Mandatory GST deps
gst_dep = dependency('gstreamer-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_base_dep'])
gstnet_dep = dependency('gstreamer-net-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_net_dep'])
gstcontroller_dep = dependency('gstreamer-controller-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_controller_dep'])


subdir('gst')
