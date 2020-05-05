CXX = g++

CFLAGS = `pkg-config gstreamer-1.0 gstreamer-app-1.0 gdk-pixbuf-2.0  --cflags` -IFlyCapture
LIBS = `pkg-config gstreamer-1.0 gstreamer-app-1.0 gdk-pixbuf-2.0 --libs` -lboost_program_options

build: gst-app-gdk-push.cpp 
	$(CXX) $(CFLAGS) gst-app-gdk-push.cpp  $(LIBS) -o gst-app-push
	
clean:
	rm gst-app-push
