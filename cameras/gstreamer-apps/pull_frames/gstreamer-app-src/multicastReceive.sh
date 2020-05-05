
# Multicast UDP receive

MULTICAST_IP_ADDR=192.168.0.103
VIDEO_UDP_PORT=5000

#CAPS="application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, sprop-parameter-sets=(string)\"Z0JAKJWgKA9k\,aM48gA\=\=\", payload=(int)96"
CAPS="application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, sprop-parameter-sets=(string)\"Z0JAKJWgKA9k\,aM48gA\=\=\", payload=(int)96"

gst-launch-1.0 udpsrc multicast-group=$MULTICAST_IP_ADDR auto-multicast=true port=$VIDEO_UDP_PORT \
caps="$CAPS" ! rtph264depay ! queue ! h264parse ! omxh264dec ! nveglglessink -e

#gst-launch-1.0 udpsrc port=5000 caps="$CAPS" \
#	! rtph264depay \
#	! queue \
#	! h264parse \
#	! omxh264dec \
#	! nveglglessink -e

