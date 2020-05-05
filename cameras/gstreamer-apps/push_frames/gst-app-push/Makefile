CXX = g++

CFLAGS = `pkg-config gstreamer-1.0 gstreamer-app-1.0 gdk-pixbuf-2.0 opencv --cflags` -I. -std=c++11
LIBS = `pkg-config gstreamer-1.0 gstreamer-app-1.0 gdk-pixbuf-2.0 opencv --libs`

build: pusher_main.cpp 
	$(CXX) $(CFLAGS) pusher_main.cpp  $(LIBS) -o pusher
	
clean:
	rm pusher
