
#gst-launch-1.0 udpsrc port=5000 ! 'application/x-rtp, encoding-name=H264, payload=96' \
! rtph264depay ! h264parse ! avdec_h264 ! xvimagesink sync=false



#CAPS="application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, sprop-parameter-sets=(string)\"Z0JAKJWgMgTflQA\=\,aM48gAA\=\", payload=(int)96"
#CAPS="application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, sprop-parameter-sets=(string)\"Z0JAIJWgFAFuQAA\=\,aM48gAA\=\", payload=(int)96"
#CAPS="application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, sprop-parameter-sets=(string)\"Z0JAKJWgKA9k\,aM48gA\=\=\", payload=(int)96"
CAPS="application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, sprop-parameter-sets=(string)\"Z0JAKJWgKA9k\,aM48gA\=\=\", payload=(int)96, ssrc=(uint)1109243108, timestamp-offset=(uint)2188558323, seqnum-offset=(uint)6856"

#/GstPipeline:pipeline0/GstUDPSink:udpsink0.GstPad:sink: caps = application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, sprop-parameter-sets=(string)"Z0JAKJWgMgTflQA\=\,aM48gAA\=", payload=(int)96, ssrc=(uint)1772236736, timestamp-offset=(uint)3174537295, seqnum-offset=(uint)12029"
#, ssrc=(uint)1772236736, timestamp-offset=(uint)3174537295, seqnum-offset=(uint)12029

#gst-launch-1.0 udpsrc port=5000 caps="$CAPS" \
#! 'application/x-rtp, encoding-name=H264, payload=96' \
#! rtph264depay ! h264parse ! omxh264dec ! xvimagesink sync=false

#gst-launch-1.0 udpsrc port=5000 caps="$CAPS" \
#! 'application/x-rtp, encoding-name=H264, payload=96' \
#! rtph264depay ! h264parse ! omxh264dec ! xvimagesink sync=false


gst-launch-1.0 udpsrc port=5000 caps="$CAPS" \
	! rtph264depay \
	! queue \
	! h264parse \
	! omxh264dec \
	! nveglglessink -e





