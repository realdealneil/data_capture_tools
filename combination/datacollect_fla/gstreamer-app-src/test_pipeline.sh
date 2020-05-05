gst-launch-1.0 videotestsrc ! \
	'video/x-raw, format=(string)GRAY8, width=(int)1280, height=(int)720' ! \
	nvvidconv ! \
	'video/x-raw(memory:NVMM),format=(string)I420' ! \
	omxh264enc bitrate=10000000 ! \
	'video/x-h264, stream-format=(string)byte-stream' ! \
	h264parse ! \
	mpegtsmux ! \
	filesink location=test.ts
	
	#qtmux ! \
	#filesink location=test.mp4 -e
