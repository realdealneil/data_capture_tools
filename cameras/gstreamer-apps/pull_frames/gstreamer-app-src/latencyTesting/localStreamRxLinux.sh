# Receive the UDP stream from doubleStream.sh (to be run on same linux computer as doubleStream.sh, a Jetson TX2)
VIDEO_UDP_PORT=5000

gst-launch-1.0 udpsrc port=$VIDEO_UDP_PORT ! "application/x-rtp" ! rtph264depay ! h264parse ! decodebin ! nveglglessink -e
