#gst-launch-1.0 -v videotestsrc \
#	! 'video/x-raw, format=(string)RGB, width=(int)800, height=(int)600' \
#	! videoconvert \
#	! omxh264enc \
#	! 'video/x-h264, stream-format=(string)byte-stream' \
#	! queue \
#	! rtph264pay pt=96 \
#	! udpsink host=192.168.0.108 port=5000

# This works:
#gst-launch-1.0 ximagesrc use-damage=0 startx=0 endx=639 starty=0 endy=479 ! ximagesink

# Test:
#gst-launch-1.0 -v ximagesrc use-damage=0 startx=0 endx=639 starty=0 endy=479 \
	#! "video/x-raw" \
	#! videoscale \
	#! videoconvert \
	#! omxh264enc \
	#! rtph264pay pt=96 \
	#! udpsink host=192.168.50.223 port=5000

# Try streaming the screen.  This will give us a decent feel for latency:
#gst-launch-1.0 -v ximagesrc use-damage=0 startx=0 endx=639 starty=0 endy=479 ! 'video/x-raw, format=(string)RGB' ! videoconvert ! omxh264enc ! 'video/x-h264, stream-format=(string)byte-stream' ! queue ! rtph264pay pt=96 ! udpsink host=192.168.50.223 port=5000

# The following worked streaming to my windows computer:
gst-launch-1.0 -v videotestsrc pattern=ball \
	! 'video/x-raw, format=(string)RGB, width=(int)640, height=(int)480' \
	! videoscale \
	! videoconvert \
	! omxh264enc \
	! 'video/x-h264, stream-format=(string)byte-stream' \
	! rtph264pay pt=96 \
	! tee name=myt \
	myt. ! queue ! udpsink host=192.168.50.57 port=5000 \
	myt. ! queue ! udpsink host=127.0.0.1 port=5000

# Receive pipeline (on windows 10 with gstreamer installed using installer):
#gst-launch-1.0 udpsrc port=5000 ! "application/x-rtp" ! rtph264depay ! h264parse ! decodebin ! autovideosink


#### Uncomment this to just grab to the screen:
#gst-launch-1.0 udpsrc port=5004 ! "application/x-rtp" ! rtph264depay ! h264parse ! omxh264dec ! nveglglessink

### Uncomment this to grab to file:
#gst-launch-1.0 udpsrc port=5004 ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, payload=(int)96, encoding-name=(string)H264" ! queue ! rtph264depay ! h264parse ! avimux ! filesink location=screenstream1.avi

#### Use this to grab to the screen and to a file:
#gst-launch-1.0 udpsrc port=5004 ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, payload=(int)96, encoding-name=(string)H264" ! \
#rtph264depay ! h264parse ! tee name=tp \
#tp. ! queue ! omxh264dec ! nveglglessink \
#tp. ! queue ! avimux ! filesink location=screenstream1.avi
