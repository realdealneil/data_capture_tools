#ifndef GST_APP_SRC_H_INCLUDED
#define GST_APP_SRC_H_INCLUDED

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <stdio.h>
//#include <gst/video/video.h>
#include "FlyCapture2.h"

#include <stdio.h>
#include <unistd.h>
#include "vectornav.h"

#include "ChrImuPacketParser.h"

#include <memory>

FlyCapture2::Property frameRateProp;
FILE* timeStampFile = NULL;

struct cameraData {
    unsigned int frameCounter;
    unsigned int embedTime;
    FlyCapture2::TimeStamp fc2Timestamp;
    double tegraTime;
	attitude_imu chrAttitude;
	attitude_imu vnAttitude;
} camData;

struct ConfigOptions // cmdline and config file options go here
{
    std::string output; // where to write the video
    std::string encoder;
    std::string encoder_name;
    int saveVideo;
    int enableDisplay;
    int iHeight;
    int iWidth;
    int frameRate;
    int bitRate;
    int iframeinterval;
    int nFramesToCapture; // number of frames to capture
    int saveMetadata;

	bool useChrImu;
	std::string chrDevice;
	int chrBaud;

	bool useVectorNav;
	std::string vnDevice;
	int vnBaud;
	int vnType;

	ConfigOptions()
	:	useChrImu(true)
	,	chrDevice("/dev/chrimu")
	,	chrBaud(921600)
	,	useVectorNav(true)
	,	vnDevice("/dev/vectornav")
	,	vnBaud(921600)
	,	vnType(2)
	{}

} co;

int want = 1;

static GMainLoop *loop;

#define WIDTH 1280
#define HEIGHT 720
//#define NUM_FRAMES_TO_CAPTURE 3600
//#define TARGET_BITRATE 10000000

//uint16_t b_white[WIDTH*HEIGHT];
//uint16_t b_black[WIDTH*HEIGHT];

//! Point Grey Camera:
FlyCapture2::Camera pg_camera;
FlyCapture2::Error pg_error;
FlyCapture2::EmbeddedImageInfo pg_embeddedInfo;
FlyCapture2::ImageMetadata pg_metadata;
FlyCapture2::CameraInfo pg_camInfo;
FlyCapture2::Format7ImageSettings fmt7ImageSettings;
FlyCapture2::Format7PacketInfo fmt7PacketInfo;
FlyCapture2::Format7Info fmt7Info;
FlyCapture2::PixelFormat k_fmt7PixFmt;
FlyCapture2::Mode k_fmt7Mode;
FlyCapture2::Image pg_rawImage;
int imageSize = 0;

class GstreamerAppSrc
{
public:
    GstreamerAppSrc()
	:	m_chrImu(NULL)
	{};
    static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);
    static void cb_need_data (GstElement *appsrc, guint unused_size, gpointer user_data);
    bool OpenCamera(int frameRate);
    int factory_make(char const* location);
    int pipeline_make();
    int watcher_make();

    GstElement *pipeline;

    GstElement *source;
    GstElement *filter1;
    GstElement *convert;
    GstElement *convFilter;
    GstElement *queue;
    GstElement *encoder;
    GstElement *filter2;
    GstElement *parser;
    GstElement *muxer;
    GstElement *sink;

    GstCaps *filter1_caps;
    GstCaps *filter2_caps;
    GstCaps *convFilterCaps;

    GstBus *bus;
    guint bus_watch_id;

    int InitImu();
	int ReadImu();

	//! Camera IMU device
    chrImuPacketParser *m_chrImu;

private:

	//! VectorNav IMU device:
    Vn100 m_vn100imu;
    Vn200 m_vn200imu;
    static void VectorNavCallback(	void* sender, VnDeviceCompositeData* data);

};

GstreamerAppSrc gstreamerAppSrc;

#endif // GST-APP-SRC_H_INCLUDED
