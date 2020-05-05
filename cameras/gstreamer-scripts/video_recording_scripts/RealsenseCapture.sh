#!/bin/bash
gst-launch-1.0 v4l2src device="/dev/video2" ! "video/x-raw, width=640, height=480, format=(string)I420" ! omxh264enc bitrate=8000000 ! 'video/x-h264, stream-format=(string)byte-stream' ! h264parse ! qtmux ! filesink location=${1:-realsense_$RANDOM}.mp4 -e
