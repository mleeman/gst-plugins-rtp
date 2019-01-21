test_rtp_sources = [
  'rtpsink',
  'rtpsrc',
]

test_rtp_dependencies = [
  gst_dep,
  gst_check_dep,
  gstrtp_dep,
]

foreach _t: test_rtp_sources
  t_bin = executable(_t,
    '@0@.c'.format(_t),
    dependencies: test_rtp_dependencies,
  )
  test('TEST : '+_t, t_bin,
    args: [ '--gst-plugin-path=@0@'.format(meson.build_root())  ],
    env: [ 'GST_DEBUG=*rtp*:5']
  )
endforeach
