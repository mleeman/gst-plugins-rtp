test_rtp_sources = [
  'rtpsink.c',
  'rtpsrc.c',
]

test_rtp_dependencies = [
  gstreamer,
  gstreamer_check,
]

foreach _t: test_rtp_sources
  t_bin = executable(_t,
    '@0@.c'.format(_t),
    dependencies: test_rtp_dependencies,
  )
  test('TEST : '+_test, t_bin,
    args: [ '--gst-plugin-path=@0@'.format(meson.build_root())  ],
    env: [ 'GST_DEBUG=*rtp*:5']
  )
endforeach
