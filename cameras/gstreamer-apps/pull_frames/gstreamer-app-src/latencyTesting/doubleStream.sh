# Stream video from a Jetson to 2 different computers: this one (localhost) and one more (RECV_IP_ADDR)
# This helps visually see how much latency is in the transmission to the second computer.  

RECV_IP_ADDR=192.168.50.57
VIDEO_UDP_PORT=5000

gst-launch-1.0 -v videotestsrc pattern=ball \
	! 'video/x-raw, format=(string)RGB, width=(int)640, height=(int)480' \
	! videoscale \
	! videoconvert \
	! omxh264enc \
	! 'video/x-h264, stream-format=(string)byte-stream' \
	! rtph264pay pt=96 \
	! tee name=myt \
	myt. ! queue ! udpsink host=$RECV_IP_ADDR port=$VIDEO_UDP_PORT \
	myt. ! queue ! udpsink host=127.0.0.1 port=$VIDEO_UDP_PORT
