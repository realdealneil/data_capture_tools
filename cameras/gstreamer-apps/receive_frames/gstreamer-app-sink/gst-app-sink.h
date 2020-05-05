#ifndef GST_APP_SRC_H_INCLUDED
#define GST_APP_SRC_H_INCLUDED

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <string>
#include <iostream>

#include <stdio.h>
//#include <gst/video/video.h>

#include <stdio.h>
#include <unistd.h>

#include <memory>

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

	ConfigOptions()
	{}

} co;

int want = 1;

static GMainLoop *loop;

#define WIDTH 1280
#define HEIGHT 720

int imageSize = 0;

class GstreamerAppSink
{
public:
    GstreamerAppSink()
	{};
    static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);
    //static void cb_need_data (GstElement *appsrc, guint unused_size, gpointer user_data);
    //bool OpenCamera(int frameRate);
    int factory_make();
    int pipeline_make();
    int watcher_make();

    GstElement *pipeline;

    GstElement *source;
    GstElement *filter1;
    GstCaps *filter1_caps;
    GstElement *depay;
    GstElement *parse;
    GstElement *decode;
    GstElement *sink;

    /*
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
    */

    GstBus *bus;
    guint bus_watch_id;

};

GstreamerAppSink gstreamerAppSink;

#endif // GST-APP-SRC_H_INCLUDED
