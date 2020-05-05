

# Video test source to h264 encoder to file:
#gst-launch-1.0 videotestsrc ! 'video/x-raw, format=(string)I420,width=(int)1280, height=(int)960' \
	! omxh264enc ! 'video/x-h264, stream-format=(string)byte-stream' ! h264parse ! qtmux \
	! filesink location=test.mp4 -e

# Try to mimic our camera a little closer:
#gst-launch-1.0 videotestsrc \
	! 'video/x-raw, format=(string)RGB, width=(int)800, height=(int)600' \
	! videoconvert \
	! omxh264enc \
	! 'video/x-h264, stream-format=(string)byte-stream' \
	! h264parse \
	! qtmux \
	! filesink location=testconvert3.mp4 \
	-e
	
#gst-launch-1.0 videotestsrc \
	! 'video/x-raw, format=(string)RGB, width=(int)800, height=(int)600' \
	! videoconvert \
	! omxh264enc \
	! 'video/x-h264, stream-format=(string)byte-stream' \
	! h264parse \
	! mpegtsmux \
	! filesink location=mp2t1.ts
	
	# -e
	
	
	#! 'video/x-h264, stream-format=(string)byte-stream'  \

# Video decode to gl display:
gst-launch-1.0 filesrc location=$1 ! qtdemux name=demux \
	demux.video_0 ! queue ! h264parse ! omxh264dec ! nveglglessink -e
