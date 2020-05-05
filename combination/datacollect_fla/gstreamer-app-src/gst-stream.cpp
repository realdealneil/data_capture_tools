// example appsrc for gstreamer 1.0 with own mainloop & external buffers. based on example from gstreamer docs.
// public domain, 2015 by Florian Echtler <floe@butterbrot.org>. compile with:
// gcc --std=c99 -Wall $(pkg-config --cflags gstreamer-1.0) -o gst gst.c $(pkg-config --libs gstreamer-1.0) -lgstapp-1.0

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <stdio.h>
//#include <gst/video/video.h>
#include "FlyCapture2.h"
#include <iostream>
#include <sstream>

#include <stdint.h>
#include <cstdlib>
//#include <stdlib.h>

#include <time.h>
#include <signal.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

double ms_time(void)
{
    struct timespec now_timespec;
    clock_gettime(CLOCK_MONOTONIC, &now_timespec);
    return ((double)now_timespec.tv_sec)*1000.0 + ((double)now_timespec.tv_nsec)*1.0e-6;
}

FlyCapture2::Property frameRateProp;
FILE* timeStampFile = NULL;

struct cameraTime {
    unsigned int frameCounter;
    unsigned int embedTime;
    FlyCapture2::TimeStamp fc2Timestamp;
    double tegraTime;
} camTime;

struct ConfigOptions // cmdline and config file options go here
{
    std::string output; // where to write the video
    std::string encoder;
    std::string encoder_name;
    int iHeight;
    int iWidth;
    int frameRate;
    int bitRate;
    int nFramesToCapture; // number of frames to capture
    int saveMetadata;
} co;

int want = 1;

static GMainLoop *loop;
GstElement *pipeline;

GstElement *source, *filter1, *accel, *encoder, *queue, *muxer, *sink;
GstElement *convert;
GstElement *filter2;
GstElement *parser;
GstElement *payloader;

GstCaps *filter1_caps, *filter2_caps;

GstBus *bus;
guint bus_watch_id;

#define WIDTH 1280
#define HEIGHT 720

#define NUM_FRAMES_TO_CAPTURE 3600

//#define TARGET_BITRATE 10000000


static void sigint_restore (void)
{
  struct sigaction action;

  memset (&action, 0, sizeof (action));
  action.sa_handler = SIG_DFL;

  sigaction (SIGINT, &action, NULL);
}


/* Signal handler for ctrl+c */
void intHandler(int dummy) {

	/* Out of the main loop, clean up nicely */
//	g_print ("Returned, stopping playback\n");
//	gst_element_set_state (pipeline, GST_STATE_NULL);
//	g_print ("Deleting pipeline\n");
//	gst_object_unref (GST_OBJECT (pipeline));
//	g_main_loop_unref (loop);

	//! Try sending an EOS signal:

	g_print("Sending EOS signal to pipeline\n");
	gst_element_send_event(pipeline, gst_event_new_eos());
	sigint_restore();
	//exit(0);
}

static gboolean bus_call	(GstBus     *bus,
							 GstMessage *msg,
							 gpointer    data)
{
//	GMainLoop *loop = (GMainLoop *) data;

	switch (GST_MESSAGE_TYPE (msg)) {

		case GST_MESSAGE_EOS:
			g_print ("End of stream\n");
			g_main_loop_quit (loop);
		break;

		case GST_MESSAGE_ERROR: {
			gchar  *debug;
			GError *error;

			gst_message_parse_error (msg, &error, &debug);
			g_free (debug);

			g_printerr ("Error: %s\n", error->message);
			g_error_free (error);

			g_main_loop_quit (loop);
		break;
		}

		default:
		break;
	}

	return TRUE;
}

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

//uint8_t buffer[WIDTH*HEIGHT*3]; // This buffer is not used...





static void cb_need_data (GstElement *appsrc, guint unused_size, gpointer user_data) {

  	static gboolean white = FALSE;
	static double last_cap_time = ms_time();
	static double frame_rate_avg = 30.0;
	static double frame_dt_ms = 33.0;
	static double last_frameRate_count_time = ms_time() - 1001.0;
	static int last_frameCount = 0;	
	static int frameCount = 0;	
	static bool once = false;
	GstBuffer *buffer;
	guint size;
	GstFlowReturn ret;

	double cap_time = ms_time();

    pg_error = pg_camera.RetrieveBuffer( &pg_rawImage );
    if (pg_error != FlyCapture2::PGRERROR_OK)
    {
        printf("Failed to capture image from PG Camera!\n");
        assert(false);
        return;
    }	

    //! It is important to get out at this stage so as to not corrupt the data in the file:
    //if (ret != GST_FLOW_OK) {
		/* something wrong, stop pushing */

	//	printf("Something went wrong.  Stop pushing!\n");
		 //g_main_loop_quit (loop);
	//	 return;
	//}

    buffer = gst_buffer_new_wrapped_full(
				(GstMemoryFlags)0,
				(gpointer)pg_rawImage.GetData(),
				imageSize,
				0,
				imageSize,
				NULL,
				NULL);

	g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
	
	pg_metadata = pg_rawImage.GetMetadata();

    //! Store the time parameters immediately:
    camTime.frameCounter = pg_metadata.embeddedFrameCounter;
    camTime.tegraTime = ms_time();
    camTime.embedTime = pg_metadata.embeddedTimeStamp;
    camTime.fc2Timestamp = pg_rawImage.GetTimeStamp();
    
    //! Log if enabled:
    if (co.saveMetadata)
    {
        //printf("Embedded time: %u\n", pg_rawImage.GetMetadata().embeddedTimeStamp);
        fprintf(timeStampFile, "[%05d, %u, %u, %lld, %u, %u, %u, %d, %lf];...\n",
            frameCount,
            camTime.frameCounter,
            camTime.embedTime,
            camTime.fc2Timestamp.seconds,
            camTime.fc2Timestamp.microSeconds,
            camTime.fc2Timestamp.cycleSeconds,
            camTime.fc2Timestamp.cycleCount,
            camTime.fc2Timestamp.cycleOffset,
            camTime.tegraTime
            );
        fflush(timeStampFile);
    }    	

	if (!once) {
		//printf("Hello once\n");
		once = true;
	} else {
		frame_dt_ms = cap_time - last_cap_time;
		if (cap_time - last_frameRate_count_time > 1000.0) {
			int framesInOneSecond = frameCount - last_frameCount;
			printf("Frame Rate: %d\n", framesInOneSecond);
			last_frameRate_count_time = cap_time;
			last_frameCount = frameCount;
		}
	}

	last_cap_time = cap_time;
    frameCount++;
	
}

bool OpenCamera(int frameRate) {
	//! Create a camera and connect to it (this is FlyCapture code):

    //! Connect Point Grey Camera:
    pg_error = pg_camera.Connect ( 0 );
    if (pg_error != FlyCapture2::PGRERROR_OK)
    {
        printf("Failed to connect to a PG Camera.  Check connection!\n");
        return false;
    }

    printf("  $$ Opened Point Grey camera!\n");

    pg_error = pg_camera.GetEmbeddedImageInfo( &pg_embeddedInfo );
    if (pg_error != FlyCapture2::PGRERROR_OK)
    {
        printf("Unable to get flycapture camera info:\n");
        pg_error.PrintErrorTrace();
        return false;
    }

    //! Turn on time stamp on PG camera if it's off:
    if ( pg_embeddedInfo.timestamp.onOff == false )
    {
        printf("PG Camera: Timestamps were off.  Turning them on!\n");
        pg_embeddedInfo.timestamp.onOff = true;
    }

    if ( pg_embeddedInfo.frameCounter.onOff == false )
    {
        printf("PG Camera: Enabling frame counter\n");
        pg_embeddedInfo.frameCounter.onOff = true;
    }

    pg_error = pg_camera.SetEmbeddedImageInfo( &pg_embeddedInfo );
    if (pg_error != FlyCapture2::PGRERROR_OK )
    {
        printf("Unable to turn on timestamp feature on PG Camera\n");
        pg_error.PrintErrorTrace();
    }

    //! Get the camera info and print it out:
    pg_error = pg_camera.GetCameraInfo( &pg_camInfo );
    if (pg_error != FlyCapture2::PGRERROR_OK )
    {
        printf("Unable to get PG Camera info\n");
        pg_error.PrintErrorTrace();
    }

    printf("Fly Capture Camera Information:\n");
    printf("  Vendor: %s\n", pg_camInfo.vendorName);
    printf("  Model: %s\n", pg_camInfo.modelName);
    printf("  Serial Num: %d\n", pg_camInfo.serialNumber);

    /// Need to find a place for these variables
    k_fmt7Mode = FlyCapture2::MODE_0;

    k_fmt7PixFmt = FlyCapture2::PIXEL_FORMAT_RGB;

    //! Check if mode is supported
    bool supported;
    fmt7Info.mode = k_fmt7Mode;
    pg_error = pg_camera.GetFormat7Info( &fmt7Info, &supported );
    if (pg_error != FlyCapture2::PGRERROR_OK )
    {
        printf("Format Mode not supported\n");
        pg_error.PrintErrorTrace();
    }
    //! Check if pixel format is supported
     if ( (k_fmt7PixFmt & fmt7Info.pixelFormatBitField) == 0 )
    {
		printf("Pixel Format not supported\n");
        pg_error.PrintErrorTrace();
    }
    //! Create struct with new settings
    fmt7ImageSettings.mode = k_fmt7Mode;
//    fmt7ImageSettings.offsetX = (fmt7Info.maxWidth-WIDTH)/2;
//    fmt7ImageSettings.offsetY = (fmt7Info.maxHeight-HEIGHT)/2;
//    fmt7ImageSettings.width = WIDTH;
//    fmt7ImageSettings.height = HEIGHT;
    fmt7ImageSettings.offsetX = (fmt7Info.maxWidth-co.iWidth)/2;
    fmt7ImageSettings.offsetY = (fmt7Info.maxHeight-co.iHeight)/2;
    fmt7ImageSettings.width = co.iWidth;
    fmt7ImageSettings.height = co.iHeight;
    fmt7ImageSettings.pixelFormat = k_fmt7PixFmt;

    //! Validate new settings
    bool valid;
    // Validate the settings to make sure that they are valid
    pg_error = pg_camera.ValidateFormat7Settings(&fmt7ImageSettings,&valid, &fmt7PacketInfo );
    if (pg_error != FlyCapture2::PGRERROR_OK )
    {
        printf("Cannot validate format settings\n");
        pg_error.PrintErrorTrace();
    }

    if ( !valid )
    {
		printf("Format settings required are not supported\n");
        pg_error.PrintErrorTrace();
        return false;
    }

    //! Set the new settings to the camera
    pg_error = pg_camera.SetFormat7Configuration(&fmt7ImageSettings,fmt7PacketInfo.recommendedBytesPerPacket );
    if (pg_error != FlyCapture2::PGRERROR_OK )
    {
        printf("Unable to set 7 camera settings\n");
        pg_error.PrintErrorTrace();
    }

    //! Start capturing images
    pg_error = pg_camera.StartCapture();
    if (pg_error == FlyCapture2::PGRERROR_ISOCH_BANDWIDTH_EXCEEDED)
    {
        printf("Bandwidth exceeded!\n");
        return false;
    }
    else if (pg_error != FlyCapture2::PGRERROR_OK)
    {
        printf("Error starting capture!\n");
        pg_error.PrintErrorTrace();
    }

    //! Capture first image to make sure things are working ok:
    pg_error = pg_camera.RetrieveBuffer( &pg_rawImage );
    if (pg_error != FlyCapture2::PGRERROR_OK)
    {
        printf("Failed to capture first image from PG Camera!\n");
        return false;
    }

    //! The user can set a different "Max Frame Rate", which will allow the autoexposure to
    //! increase shutter speed.
    frameRateProp.type = FlyCapture2::FRAME_RATE;
    if (frameRate == 0)
    {
        frameRateProp.onOff = true;
        frameRateProp.autoManualMode = true;
        frameRateProp.absControl = false;
    }
    else
    {
        frameRateProp.onOff = true;
        frameRateProp.autoManualMode = false;
        frameRateProp.absControl = true;
        frameRateProp.absValue = (float)frameRate;
    }
    pg_error = pg_camera.SetProperty(&frameRateProp);
    if ( pg_error != FlyCapture2::PGRERROR_OK )
    {
        printf("Error setting frame rate on camera!\n");
        pg_error.PrintErrorTrace();
        return false;
    }

    //! Create dump file if we are saving images:
    imageSize = pg_rawImage.GetDataSize();

    printf("  IMAGE SIZE: %d\n", imageSize);
    
    
    //! Make file for timestamps:
    if (co.saveMetadata) {
		std::stringstream ss("");
		ss << co.output << ".m";		
		timeStampFile = fopen(ss.str().c_str(), "w+");
		if (timeStampFile == NULL)
		{
			printf("Error creating timestamp file\n");			
			return false;
		}

		fprintf(timeStampFile, "captureHeaders = {'Index', 'PG_Index', 'PG_embedTime', 'PG_Time_Sec', 'PG_microsec', "
			"'Cycle_seconds', 'Cycle_count', 'Cycle_offset', 'tegra_time'"
			"};\n");
		fprintf(timeStampFile, "captureData = [...\n");
	}
	return true;
}

int factory_make(char const* location)
{
	//! Encode to file:
	//! This pipeline works well.  Now, to hook it up to our appsrc:
	//gst-launch-1.0 videotestsrc \
	! 'video/x-raw, format=(string)RGB, width=(int)800, height=(int)600' \
	! videoconvert \
	! omxh264enc \
	! 'video/x-h264, stream-format=(string)byte-stream' \
	! h264parse \
	! qtmux \
	! filesink location=testconvert2.mp4 -e
	
	
	//! Streaming pipeline:
	//gst-launch-1.0 -v videotestsrc \
	! 'video/x-raw, format=(string)RGB, width=(int)800, height=(int)600' \
	! videoconvert \
	! omxh264enc \
	! 'video/x-h264, stream-format=(string)byte-stream' \
	! queue \
	! rtph264pay pt=96 \
	! udpsink host=192.168.0.108 port=5000
	
	
	
	/* Create gstreamer elements */
	pipeline 	= gst_pipeline_new ("pipeline");
	source		= gst_element_factory_make ("appsrc", "source");
	//source   	= gst_element_factory_make ("videotestsrc",	"videotestsrc");
	filter1  	= gst_element_factory_make ("capsfilter",	"filter1");
	convert	 	= gst_element_factory_make ("videoconvert", "conv");
//	encoder  	= gst_element_factory_make ("omxh264enc",	"h264");
    printf("Using encoder %s\n", co.encoder.c_str());
	encoder  	= gst_element_factory_make (co.encoder.c_str(), co.encoder_name.c_str());
	filter2	 	= gst_element_factory_make ("capsfilter", "filter2");
	queue	 	= gst_element_factory_make ("queue", "myqueue");
	payloader 	= gst_element_factory_make ("rtph264pay", "payloader");
	sink		= gst_element_factory_make ("udpsink", "sink");
	
	//parser	 = gst_element_factory_make ("h264parse", "parser");
	
	
	
	
	//! Note: qtmux is good if you can cleanly exit the app and send EOS upstream.  Otherwise, your files are corrupted.
	//muxer    = gst_element_factory_make ("qtmux",			"mux");
	//GstElement *muxer	 = gst_element_factory_make ("mpegtsmux", "mpegmux");
	//sink     = gst_element_factory_make ("filesink",		"filesink");
	//GstElement *fakesink = gst_element_factory_make ("fakesink", "fakey");

	//! setup
	g_object_set (G_OBJECT (source), "caps",
		gst_caps_new_simple ("video/x-raw",
					 "format", G_TYPE_STRING, "RGB",
					 "width", G_TYPE_INT, co.iWidth,
					 "height", G_TYPE_INT, co.iHeight,
					 "framerate", GST_TYPE_FRACTION, 0, 1,
					 NULL), NULL);
//	g_object_set (G_OBJECT (source), "caps",
//		gst_caps_new_simple ("video/x-raw",
//					 "format", G_TYPE_STRING, "RGB",
//					 "width", G_TYPE_INT, WIDTH,
//					 "height", G_TYPE_INT, HEIGHT,
//					 "framerate", GST_TYPE_FRACTION, 0, 1,
//					 NULL), NULL);

	/* Video caps */
	filter1_caps = gst_caps_new_simple ("video/x-raw",
		"format", G_TYPE_STRING, "RGB",
		"width", G_TYPE_INT, co.iWidth,
		"height", G_TYPE_INT, co.iHeight,
		NULL);
//	filter1_caps = gst_caps_new_simple ("video/x-raw",
//		"format", G_TYPE_STRING, "RGB",
//		"width", G_TYPE_INT, WIDTH,
//		"height", G_TYPE_INT, HEIGHT,
//		NULL);

	filter2_caps = gst_caps_new_simple ("video/x-h264",
		"stream-format", G_TYPE_STRING, "byte-stream",
		NULL);

	if (!pipeline) {
		g_printerr ("Pipeline could not be created. Exiting.\n");
		return -1;
	}
	if (!source) {
		g_printerr ("Source could not be created. Exiting.\n");
		return -1;
	}
	if (!filter1) {
		g_printerr ("Filter 1 could not be created. Exiting.\n");
		return -1;
	}
	if (!convert) {
		g_printerr ("Convert element could not be created. Exiting.\n");
		return -1;
	}
	if (!encoder) {
		g_printerr ("Encoder element could not be created. Exiting.\n");
		return -1;
	}
	if (!filter2) {
		g_printerr ("Filter 2 element could not be created. Exiting.\n");
		return -1;
	}
	/*if (!parser) {
		g_printerr ("Parser element could not be created. Exiting.\n");
		return -1;
	}
	if (!muxer) {
		g_printerr ("Muxer element could not be created. Exiting.\n");
		return -1;
	}*/
	if (!sink) {
		g_printerr ("Sink element could not be created. Exiting.\n");
		return -1;
	}
	
	if (!payloader) {
		g_printerr ("Error making payloader element\n");
		return -1;
	}
	
	if (!filter1_caps) {
		g_printerr ("Filter1 caps could not be created. Exiting.\n");
		return -1;
	}
	
	if (!filter2_caps) {
		g_printerr ("Filter 2 caps could not be created. Exiting.\n");
		return -1;
	}

	/* Set up elements */
	g_object_set (G_OBJECT (filter1), "caps", filter1_caps, NULL);
	g_object_set (G_OBJECT (filter2), "caps", filter2_caps, NULL);
	gst_caps_unref (filter1_caps);
	gst_caps_unref (filter2_caps);
//	g_object_set (G_OBJECT (source), "num-buffers", NUM_FRAMES_TO_CAPTURE, NULL);
	g_object_set (G_OBJECT (source), "num-buffers", co.nFramesToCapture, NULL);

	/*int target_bitrate = atoi(bitrate_arg);

	if (target_bitrate < 100000) {
		printf("Bitrate too low! setting to 100000\n");
		target_bitrate = 100000;
	} else if (target_bitrate > 100000000) {
		printf("Bitrate is ridiculous!  Setting to 100 Mbps\n");
		target_bitrate = 100000000;
	}*/

	//! Settings for the h264 encoder:
	g_object_set (G_OBJECT (encoder), "bitrate", co.bitRate, NULL);

	/* Settings for payloader: */
	g_object_set (G_OBJECT (payloader), "pt", 96, NULL);

	/* we set the input filename to the source element */
	//g_object_set (G_OBJECT (sink), "location", location, NULL);
	//! UDP SINK:
	g_object_set (G_OBJECT (sink), "host", "192.168.0.108", "port", 5000, NULL);

	// Fixing intermittentcorrupted file issue
	//g_object_set(G_OBJECT (muxer), "faststart", TRUE, NULL);
	//g_object_set(G_OBJECT (muxer), "fragment-duration", 500, NULL);


	/* setup appsrc */
	g_object_set (G_OBJECT (source),
		"stream-type", GST_APP_STREAM_TYPE_STREAM,
		"format", GST_FORMAT_TIME,
		"is-live", TRUE,
		"do-timestamp", TRUE,
		NULL);
	g_signal_connect (source, "need-data", G_CALLBACK (cb_need_data), NULL);

	return 0;
}

int pipeline_make()
{
	//! Streaming pipeline:
	//gst-launch-1.0 -v videotestsrc \
	! 'video/x-raw, format=(string)RGB, width=(int)800, height=(int)600' \
	! videoconvert \
	! omxh264enc \
	! 'video/x-h264, stream-format=(string)byte-stream' \
	! queue \
	! rtph264pay pt=96 \
	! udpsink host=192.168.0.108 port=5000
	
	
	/* we add all elements into the pipeline */
	gst_bin_add_many (GST_BIN (pipeline),
		source, filter1, convert, encoder, filter2, queue, payloader, sink, NULL);
	/* we link the elements together */
	gst_element_link_many (source, filter1, convert, encoder, filter2, queue, payloader, sink, NULL);
	
	//! To get verbose output (needed for setting up video streaming)
	g_signal_connect( pipeline, "deep-notify", G_CALLBACK( gst_object_default_deep_notify ), NULL );

	return 0;
}

int watcher_make()
{
	/* we add a message handler */
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
	gst_object_unref (bus);
	return 0;
}

gint main (gint argc, gchar *argv[]) {

	//GstElement *pipeline, , *queue, *videosink;
	signal(SIGINT, intHandler);

//    struct sigaction sigact;
//    sigact.sa_handler = intHandler;
//    sigemptyset(&sigact.sa_mask);
//    sigact.sa_flags   = 0;
//    sigaction(SIGINT, &sigact, 0);

//	/* Check input arguments */
//	if (argc != 2) {
//		g_printerr ("Usage: %s <Recorded file name eg: test.h264>\n", argv[0]);
//		return -1;
//	}

    try
    {
        /**
            Options that can only be specified on the command line:
         */
        po::options_description cmdline_only_opts("Command Line Options");
        cmdline_only_opts.add_options()
            ("help",        "produce help message and exit")
        ;

        po::options_description config_opts("Configuration Options");
        config_opts.add_options()
            ("output-file,o",
                po::value<std::string>(&(co.output))->default_value("videoFile"),
                "output file name, ex: vga_output1.vga")
//            ("encoder,e",
//                po::value<std::string>(&(co.encoder))->default_value("h264"),
//                "encoder type: h264, mpeg4, vp8")
            ("width,w",
                po::value<int>(&(co.iWidth))->default_value(1280),
                "Image capture width (pixels)")
            ("height,h",
                po::value<int>(&(co.iHeight))->default_value(720),
                "Image capture height (pixels)")
            ("frameRate,r",
                po::value<int>(&(co.frameRate))->default_value(0),
                "Frame Rate (Integer) (0=no limit, >0 may help auto exposure work better)")
            ("bitRate,b",
                po::value<int>(&(co.bitRate))->default_value(10000000),
                "Bit Rate (Integer), ex: 10000000")
            ("nFrames,n",
                po::value<int>(&(co.nFramesToCapture))->default_value(1000),
                "Number of frames to capture (Integer), ex: 1000")
            ("saveMetadata,s",
				po::value<int>(&(co.saveMetadata))->default_value(1),
				"Save Metadata (timestamps, etc, in an m-file)")
            ;

        //! If an option is not prepended by a flag in the arguments list, treat it as an output file:
        po::positional_options_description p;
        p.add("output-file", -1);
        po::options_description cmdline_opts;
        cmdline_opts.add(cmdline_only_opts).add(config_opts);
         //! This is the map of variables that can be used to access the arguments:
        po::variables_map vm;
        //! Parse the commandline:
        po::store(po::command_line_parser(argc, argv).options(cmdline_opts).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << "Usage: " << argv[0] << " [options] -o output-file\n";
            std::cout << cmdline_opts << "\n";
            exit(0);
        }

        // All params have dafaults
        co.bitRate = vm["bitRate"].as<int>();
//        if(boost::iequals(co.encoder, "mpeg4"))
//        {
//            co.encoder = "avenc_mpeg4";
//            co.encoder_name = "mp4";
//        }
//        else if(boost::iequals(co.encoder, "vp8"))
//        {
//            co.encoder = "omxvp8enc";
//            co.encoder_name = "vp8";
//        }
//        else
//        {
//            printf("Defaulting to omxh264enc encoder (h.264)\n");
//            co.encoder = "omxh264enc";
//            co.encoder_name = "h264";
//        }
        co.encoder = "omxh264enc";
        co.encoder_name = "h264";
        co.frameRate = vm["frameRate"].as<int>();
        co.iHeight = vm["height"].as<int>();
        co.iWidth = vm["width"].as<int>();
        co.nFramesToCapture = vm["nFrames"].as<int>();
        co.output = vm["output-file"].as<std::string>();
    }
    catch(std::exception &e)
    {
        std::cout << "Commandline parsing exception: " << e.what() << std::endl;
        return -1;
    }

	if (!OpenCamera(co.frameRate))
	{
	  return -1;
	}

	/* init GStreamer */
	gst_init (&argc, &argv);

	loop = g_main_loop_new (NULL, FALSE);



	/* Check input arguments */
//	if (argc < 3) {
//		g_printerr ("Usage: %s <Recorded file name eg: test.h264> target_bitrate\n", argv[0]);
//		return -1;
//	}

	char const* output_location = co.output.c_str();
	if(factory_make(output_location) != 0)
 		return -1;
//	if(factory_make(argv[1]) != 0)
//		return -1;

	/* Add function to watch bus */
	if(watcher_make() != 0)
		return -1;

	/* Add elements to pipeline, and link them */
	if(pipeline_make() != 0)
		return -1;

	/* play */
	gst_element_set_state (pipeline, GST_STATE_PLAYING);

	g_main_loop_run(loop);

	/* clean up */
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (pipeline));
	g_main_loop_unref (loop);
	
	//! Close log file:
	if (co.saveMetadata) {
		fprintf(timeStampFile, "];\n");
        fclose(timeStampFile);
        timeStampFile = NULL;
	}

	return 0;
}
