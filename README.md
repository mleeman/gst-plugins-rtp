# gst-plugins-rtp

This is a re-implementation of the RTP elements that are submitted in
2013 to handle RTP streams. The elements handle a correct connection
for the bi-directional use of the RTCP sockets.

https://bugzilla.gnome.org/show_bug.cgi?id=703111

The rtp_rtpsink and rtp_rtpsrc elements add an URI interface so that streams
can be decoded with decodebin using the rtp:// interface.

The code can be used as follows

```
gst-launch-1.0 videotestsrc ! x264enc ! rtph264pay config-interval=3 ! rtp_rtpsink uri=rtp://239.1.1.1:1234
gst-play-1.0 rtp://239.1.1.1:1234

gst-launch-1.0 videotestsrc ! x264enc ! rtph264pay config-interval=1 ! rtp_rtpsink uri=rtp://239.1.2.3:5000
gst-launch-1.0 rtp_rtpsrc uri=rtp://239.1.2.3:5000?encoding-name=H264 ! rtph264depay ! avdec_h264 ! videoconvert ! xvimagesink

gst-launch-1.0 videotestsrc ! avenc_mpeg4 ! rtpmp4vpay config-interval=1 ! rtp_rtpsink uri=rtp://239.1.2.3:5000
gst-launch-1.0 rtp_rtpsrc uri=rtp://239.1.2.3:5000?encoding-name=MP4V-ES ! rtpmp4vdepay ! avdec_mpeg4 ! videoconvert ! xvimagesink

```
