#include "gst-app-push.h"

#include <unistd.h>

/**
 *  double ms_time(void): returns the current time as a double floating point
 *  time since an arbitrary initial time.  We use the monotonic timer which can't
 *  go backwards.
 */
static inline double ms_time(void)
{
    struct timespec now_timespec;
    clock_gettime(CLOCK_MONOTONIC, &now_timespec);
    return ((double)now_timespec.tv_sec)*1000.0 + ((double)now_timespec.tv_nsec)*1.0e-6;
}

int main(int argc, char *argv[])
{	
	GstAppPush::configOpts opt;
	opt.iWidth = 244;
	opt.iHeight = 156;
	
	GstAppPush pusher;	
	pusher.Start(opt);
	
	//! Do stuff
	uint8_t b = 127;
	
	//! Create an image and publish it!
	cv::Size matSize = cv::Size(opt.iWidth, opt.iHeight);
		
	for (int i=0; i<100; i++) {
		pusher.PushImage(cv::Mat::ones(matSize, CV_8UC1)*b);
		b += 5;
		usleep(30000);
	}
	
	pusher.Close();
	
	printf("All done\n");
	
	
}
