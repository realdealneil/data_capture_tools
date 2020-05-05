#!/bin/bash

# x86 Ubuntu 16.04 Transmitter:
gst-launch-1.0 -v videotestsrc pattern=ball ! \
	'video/x-raw, format=(string)RGB, width=(int)640, height=(int)480' ! \
	videoscale ! videoconvert ! x264enc tune=zerolatency ! rtph264pay pt=96 ! udpsink host=127.0.0.1 port=5000
