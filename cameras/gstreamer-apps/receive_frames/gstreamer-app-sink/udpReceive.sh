#!/bin/sh

#### Uncomment this to just grab to the screen:
#gst-launch-1.0 udpsrc port=5004 ! "application/x-rtp" ! rtph264depay ! h264parse ! omxh264dec ! nveglglessink

### Uncomment this to grab to file:
#gst-launch-1.0 udpsrc port=5004 ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, payload=(int)96, encoding-name=(string)H264" ! queue ! rtph264depay ! h264parse ! avimux ! filesink location=screenstream1.avi

#### Use this to grab to the screen and to a file:
gst-launch-1.0 udpsrc port=5004 ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, payload=(int)96, encoding-name=(string)H264" ! \
rtph264depay ! h264parse ! tee name=tp \
tp. ! queue ! omxh264dec ! nveglglessink \
tp. ! queue ! avimux ! filesink location=screenstream1.avi







### Stuff that didn't work:
## This didn't work (made a file, but couldn't play it back on Jetson)
#gst-launch-1.0 udpsrc port=5004 ! "application/x-rtp" ! filesink location=screenstream1.m2ts

# "application/x-rtp, media=(string)video, clock-rate=(int)90000, payload=(int)96, encoding-name=(string)H264" ! rtph264depay ! fakesink 
#! avimux ! filesink location=screenstream1.avi

### tp. ! filesink location=screenstream1.m2ts

