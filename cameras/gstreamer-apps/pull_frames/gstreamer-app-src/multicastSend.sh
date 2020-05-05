
# Multicast UDP send

MULTICAST_IP_ADDR=192.168.0.103
VIDEO_UDP_PORT=5000

gst-launch-1.0 -v v4l2src ! video/x-raw, width=640, height=480 \
! omxh264enc ! h264parse ! rtph264pay \
! udpsink host=$MULTICAST_IP_ADDR port=$VIDEO_UDP_PORT auto-multicast=true 
#gst-launch-1.0 -v v4l2src ! video/x-raw, width=640, height=480 \
#! omxh264enc ! h264parse ! rtph264pay \
#! udpsink host=$MULTICAST_IP_ADDR port=$VIDEO_UDP_PORT auto-multicast=true dem. \
#! queue ! rtpmp4apay ! udpsink host=$MULTICAST_IP_ADDR port=$AUDIO_UDP_PORT auto-multicast=true

#gst-launch-1.0 -v v4l2src	 ! video/x-raw, width=640, height=480 \
#	! omxh264enc ! h264parse ! rtph264pay ! udpsink host=192.168.0.103 port=5000
