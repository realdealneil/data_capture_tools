CXX = g++

CFLAGS = `pkg-config gstreamer-1.0 gstreamer-app-1.0  --cflags` -IFlyCapture
LIBS = `pkg-config gstreamer-1.0 gstreamer-app-1.0 --libs` -lflycapture -lboost_program_options

build: gst-app-src.cpp gst-stream.cpp
	$(CXX) $(CFLAGS) gst-app-src.cpp $(LIBS) -o gst-app-src
	$(CXX) $(CFLAGS) gst-stream.cpp $(LIBS) -o gst-stream
	
clean:
	rm gst-app-src
