// example appsrc for gstreamer 1.0 with own mainloop & external buffers. based on example from gstreamer docs.
// public domain, 2015 by Florian Echtler <floe@butterbrot.org>. compile with:
// gcc --std=c99 -Wall $(pkg-config --cflags gstreamer-1.0) -o gst gst.c $(pkg-config --libs gstreamer-1.0) -lgstapp-1.0

#include "gst-app-src.h"
#include <iostream>
#include <sstream>

#include <stdint.h>
#include <cstdlib>
//#include <stdlib.h>

#include <time.h>
#include <signal.h>

#include "flaUtilities.h"

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

attitude_imu g_latestVectorNavAttitude;
attitude_imu g_latestChrAttitude;

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
	gst_element_send_event(gstreamerAppSrc.pipeline, gst_event_new_eos());
	sigint_restore();
	//exit(0);
}

gboolean GstreamerAppSrc::bus_call (GstBus *bus, GstMessage *msg, gpointer data)
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

void GstreamerAppSrc::cb_need_data (GstElement *appsrc, guint unused_size, gpointer user_data)
{
	static double last_frameRate_count_time = ms_time() - 1001.0;
	static int last_frameCount = 0;
	static int frameCount = 0;
	static bool once = false;
	GstBuffer *buffer;
	GstFlowReturn ret;

	double cap_time = ms_time();

    pg_error = pg_camera.RetrieveBuffer( &pg_rawImage );
    if (pg_error != FlyCapture2::PGRERROR_OK)
    {
        printf("Failed to capture image from PG Camera!\n");
        assert(false);
        return;
    }

    //! Read IMU(s):
    camData.tegraTime = ms_time();

    //! Read IMU:
    if (co.useChrImu)
    {
		gstreamerAppSrc.ReadImu();
		gstreamerAppSrc.m_chrImu->getLastAttitudeSample(camData.chrAttitude);
		/*printf("PG: Read IMU: roll: %f pitch: %f yaw: %f\n",
			camData.chrAttitude.phi_deg,
			camData.chrAttitude.theta_deg,
			camData.chrAttitude.psi_deg);*/
	}

	if (co.useVectorNav) {
        camData.vnAttitude = g_latestVectorNavAttitude;
	}

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
    camData.frameCounter = pg_metadata.embeddedFrameCounter;
    camData.embedTime = pg_metadata.embeddedTimeStamp;
    camData.fc2Timestamp = pg_rawImage.GetTimeStamp();

    //! Log if enabled:
    if (co.saveMetadata)
    {
        //printf("Embedded time: %u\n", pg_rawImage.GetMetadata().embeddedTimeStamp);
        fprintf(timeStampFile, "[%05d, %u, %u, %lld, %u, %u, %u, %d, %lf"
            " %f %f %f"
            " %f %f %f"
			"];...\n",
            frameCount,
            camData.frameCounter,
            camData.embedTime,
            camData.fc2Timestamp.seconds,
            camData.fc2Timestamp.microSeconds,
            camData.fc2Timestamp.cycleSeconds,
            camData.fc2Timestamp.cycleCount,
            camData.fc2Timestamp.cycleOffset,
            camData.tegraTime,
			//! CHR IMU Data:
            camData.chrAttitude.phi_deg,
			camData.chrAttitude.theta_deg,
			camData.chrAttitude.psi_deg,
			//! VectorNav Data:
			camData.vnAttitude.phi_deg,
			camData.vnAttitude.theta_deg,
			camData.vnAttitude.psi_deg


            );
        fflush(timeStampFile);
    }

	if (!once)
	{
		//printf("Hello once\n");
		once = true;
	}
	else
	{
		if (cap_time - last_frameRate_count_time > 1000.0)
		{
			int framesInOneSecond = frameCount - last_frameCount;
			printf("Frame Rate: %d\n", framesInOneSecond);
			last_frameRate_count_time = cap_time;
			last_frameCount = frameCount;
		}
	}

    frameCount++;
}

/**
    Initialize the IMU: You can zero the gyros here, but that may not be desirable.
    You can also reset the EKF.  We will do that by default, but require a program option
    to zero gyros on startup.
 */
int GstreamerAppSrc::InitImu()
{
    printf("OPENING IMUs!!!!\n\n");

    if (co.useChrImu)
    {

        /**
            Instantiate IMU class
         */
        try
        {
            m_chrImu = new chrImuPacketParser(co.chrDevice, co.chrBaud);
            printf("  ^^^^^ Opening IMU Serial: %s, %d\n", co.chrDevice.c_str(), co.chrBaud);
        }
        catch(std::exception &e)
        {
            m_chrImu = NULL;
            std::cout << "Error creating imu class: " << e.what() << std::endl;
            return -1;
        }

        //! Check that the IMU serial port opened properly:
        if (!m_chrImu->portIsOpen())
        {
            printf("Error opening IMU Serial Port.\n");
            return -1;
        }

        /*if (m_pgSettings.savePathCreated)
        {
            m_chrImu->setupLogging(m_pgSettings.saveImagePath);
        }*/

        //! Reset the EKF on startup:
        m_chrImu->sendResetEkfPacket();
        usleep(1000);
    }

    if (co.useVectorNav)
    {
        printf("*********** Starting up VECTOR NAV!!!!!!!!!!!!!!!!!!!!!!!\n");

        VN_ERROR_CODE errorCode;

        if (co.vnType == 2)
        {

            errorCode = vn200_connect(
                &m_vn200imu,
                co.vnDevice.c_str(),
                co.vnBaud);

            /** Make sure the user has permission to use the COM port. */
            if (errorCode == VNERR_PERMISSION_DENIED) {

                printf("Current user does not have permission to open the COM port for the VectorNav!\n");
                printf("Try running again using 'sudo'.\n");

                return -1;
            }
            else if (errorCode != VNERR_NO_ERROR)
            {
                printf("Error encountered when trying to connect to the VectorNav IMU sensor.\n");
                return -1;
            }

            /** Configure the VN-200 to output asynchronous data. */
            errorCode = vn200_setAsynchronousDataOutputType(
                &m_vn200imu,
                VNASYNC_VNINS,
                true);

            /** Now tell the library which function to call when a new asynchronous
                packet is received. */
            errorCode = vn200_registerAsyncDataReceivedListener(
                &m_vn200imu,
                //&vnAsyncDataListener);
                &GstreamerAppSrc::VectorNavCallback);
        } else if (co.vnType == 1) {

            errorCode = vn100_connect(
                &m_vn100imu,
                co.vnDevice.c_str(),
                co.vnBaud);

            /** Make sure the user has permission to use the COM port. */
            if (errorCode == VNERR_PERMISSION_DENIED) {

                printf("Current user does not have permission to open the COM port for the VectorNav!\n");
                printf("Try running again using 'sudo'.\n");

                return -1;
            }
            else if (errorCode != VNERR_NO_ERROR)
            {
                printf("Error encountered when trying to connect to the VectorNav IMU sensor.\n");
                return -1;
            }

            /* Configure the VN-100 to output asynchronous data. */
            errorCode = vn100_setAsynchronousDataOutputType(
                &m_vn100imu,
                VNASYNC_VNYPR,
                true);

            /* Now tell the library which function to call when a new asynchronous
               packet is received. */
            errorCode = vn100_registerAsyncDataReceivedListener(
                &m_vn100imu,
                &GstreamerAppSrc::VectorNavCallback);
        } else {
            assert(0);
        }


    }



    return 0;
}

void GstreamerAppSrc::VectorNavCallback(	void* sender, VnDeviceCompositeData* data)
{
    //printf(" VN200 New: Roll, Pitch, Yaw (degrees) %+#7.2f %+#7.2f %+#7.2f\n",
	//	data->ypr.roll,
	//	data->ypr.pitch,
	//	data->ypr.yaw);
    g_latestVectorNavAttitude.tegra_timestamp_ms = ms_time();
    g_latestVectorNavAttitude.phi_deg = data->ypr.roll;
    g_latestVectorNavAttitude.theta_deg = data->ypr.pitch;
    g_latestVectorNavAttitude.psi_deg = data->ypr.yaw;
}

/**
    Read the IMU
 */
int GstreamerAppSrc::ReadImu()
{
    if (!co.useChrImu)
    {
        //! This is not an error.  The user didn't turn on the IMU.
        return 0;
    }

    //! This is a work-around for when image processing takes a long time!  TODO: Get rid of this at some point!
    int ret = m_chrImu->ProcessBytes();
    int count = 1;
    while (ret == -CHR_PACKET_ERROR_FLUSH_INPUT)
    {
        ret = m_chrImu->ProcessBytes();
        count++;
    }

    if (count > 1)
    {
        printf("NVXIO PG IMU: Had to process the imu serial port %d times\n", count);
    }
    return ret;
}


#define USE_MONOCHROME_CAPTURE 1

bool GstreamerAppSrc::OpenCamera(int frameRate)
{
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

	#if USE_MONOCHROME_CAPTURE
		k_fmt7PixFmt = FlyCapture2::PIXEL_FORMAT_MONO8;
	#else
		k_fmt7PixFmt = FlyCapture2::PIXEL_FORMAT_RGB;
	#endif

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

		fprintf(timeStampFile, "captureHeaders = {'Index', 'PG_Index', 'PG_embedTime', 'PG_Time_Sec', 'PG_microsec'"
			", 'Cycle_seconds', 'Cycle_count', 'Cycle_offset', 'tegra_time'"
			", 'chr_phi', 'chr_theta', 'chr_psi'"
			", 'vn_phi', vn_theta', 'vn_psi'"
			"};\n");
		fprintf(timeStampFile, "captureData = [...\n");
	}

	InitImu();
	return true;
}
#define USE_TESTSRC 0
//#define USE_OLD_VIDEOCONVERT 0
#define USE_FAKESINK 0


int GstreamerAppSrc::factory_make(char const* location)
{
	/* Create gstreamer elements */
	gstreamerAppSrc.pipeline = gst_pipeline_new ("pipeline");
#if USE_TESTSRC
	//! Test with a videotestsrc to recreate a working pipeline.  Then hook back up with the appsrc we want
	gstreamerAppSrc.source = gst_element_factory_make ("videotestsrc", "source");
#else
	gstreamerAppSrc.source = gst_element_factory_make ("appsrc", "source");
	//! setup
	g_object_set (G_OBJECT (gstreamerAppSrc.source), "caps",
		gst_caps_new_simple ("video/x-raw",
					 "format", G_TYPE_STRING, (USE_MONOCHROME_CAPTURE) ? "GRAY8" : "RGB",
					 "width", G_TYPE_INT, co.iWidth,
					 "height", G_TYPE_INT, co.iHeight,
					 "framerate", GST_TYPE_FRACTION, 0, 1,
					 NULL), NULL);

	/* setup appsrc */
	g_object_set (G_OBJECT (gstreamerAppSrc.source),
		"stream-type", GST_APP_STREAM_TYPE_STREAM,
		"format", GST_FORMAT_TIME,
		"is-live", TRUE,
		"do-timestamp", TRUE,
		NULL);
	g_signal_connect(gstreamerAppSrc.source, "need-data", G_CALLBACK (cb_need_data), &gstreamerAppSrc);
#endif



	//! Alternate sources for test purposes:
    //source   = gst_element_factory_make ("videotestsrc",	"videotestsrc");
	gstreamerAppSrc.filter1  = gst_element_factory_make ("capsfilter",	"filter1");

	/* Video caps */
	gstreamerAppSrc.filter1_caps = gst_caps_new_simple ("video/x-raw",
		"format", G_TYPE_STRING, (USE_MONOCHROME_CAPTURE) ? "GRAY8" : "RGB",
		"width", G_TYPE_INT, co.iWidth,
		"height", G_TYPE_INT, co.iHeight,
		NULL);

	if (!gstreamerAppSrc.filter1_caps) {
		g_printerr ("Filter1 caps could not be created. Exiting.\n");
		return -1;
	}

	g_object_set (G_OBJECT (gstreamerAppSrc.filter1), "caps", gstreamerAppSrc.filter1_caps, NULL);
	gst_caps_unref (gstreamerAppSrc.filter1_caps);

#if USE_MONOCHROME_CAPTURE

	// Reproduce this part of the pipeline:
	//nvvidconv ! 'video/x-raw(memory:NVMM),format=(string)I420'

	gstreamerAppSrc.convert	 = gst_element_factory_make ("nvvidconv", "nvvidconv1");
	gstreamerAppSrc.convFilter = gst_element_factory_make ("capsfilter", "convfilter");


	/* caps for output of nvvidconv: */
	gstreamerAppSrc.convFilterCaps = gst_caps_new_simple (
		"video/x-raw(memory:NVMM)",
		"format", G_TYPE_STRING, "I420",
		NULL);

	/*if (!gstreamerAppSrc.convFilterCaps) {
		g_printerr( "convFilterCaps could not be created. Exiting.\n");
		return -1;
	}*/

	g_object_set (G_OBJECT (gstreamerAppSrc.convFilter), "caps", gstreamerAppSrc.convFilterCaps, NULL);
	gst_caps_unref (gstreamerAppSrc.convFilterCaps);
#else
	//! Not sure how to use nvidia's converter tool for RGB video.  YUV seems to work, but not RGB
	gstreamerAppSrc.convert	 = gst_element_factory_make ("videoconvert", "conv");
#endif

	gstreamerAppSrc.queue	 = gst_element_factory_make ("queue", "myqueue");
//	encoder  = gst_element_factory_make ("omxh264enc",	"h264");
    printf("Using encoder %s\n", co.encoder.c_str());
	gstreamerAppSrc.encoder  = gst_element_factory_make (co.encoder.c_str(), co.encoder_name.c_str());
	gstreamerAppSrc.filter2	 = gst_element_factory_make ("capsfilter", "filter2");
	gstreamerAppSrc.parser	 = gst_element_factory_make ("h264parse", "parser");
	//! Note: qtmux is good if you can cleanly exit the app and send EOS upstream.  Otherwise, your files are corrupted.
	gstreamerAppSrc.muxer    = gst_element_factory_make ("qtmux",			"mux");
	//GstElement *muxer	 = gst_element_factory_make ("mpegtsmux", "mpegmux");


	//! Set up the sink!!!
#if USE_FAKESINK
	gstreamerAppSrc.sink = gst_element_factory_make ("fakesink", "fakey");
#else
	if (co.saveVideo) {
		gstreamerAppSrc.sink     = gst_element_factory_make ("filesink",		"filesink");
		/* we set the input filename to the source element */
		g_object_set (G_OBJECT (gstreamerAppSrc.sink), "location", location, NULL);
	} else {
		if (co.enableDisplay) {
			//gstreamerAppSrc.sink = gst_element_factory_make ("xvimagesink", "xvimagesink");
			gstreamerAppSrc.sink = gst_element_factory_make ("nveglglessink", "xvimagesink");
		} else {
			gstreamerAppSrc.sink = gst_element_factory_make ("fakesink", "fakey");
		}
	}
#endif

//	filter1_caps = gst_caps_new_simple ("video/x-raw",
//		"format", G_TYPE_STRING, "RGB",
//		"width", G_TYPE_INT, WIDTH,
//		"height", G_TYPE_INT, HEIGHT,
//		NULL);

	gstreamerAppSrc.filter2_caps = gst_caps_new_simple ("video/x-h264",
		"stream-format", G_TYPE_STRING, "byte-stream",
		NULL);

	if (!gstreamerAppSrc.pipeline) {
		g_printerr ("Pipeline could not be created. Exiting.\n");
		return -1;
	}
	if (!gstreamerAppSrc.source) {
		g_printerr ("Source could not be created. Exiting.\n");
		return -1;
	}
	if (!gstreamerAppSrc.filter1) {
		g_printerr ("Filter 1 could not be created. Exiting.\n");
		return -1;
	}
	if (!gstreamerAppSrc.convert) {
		g_printerr ("Convert element could not be created. Exiting.\n");
		return -1;
	}
	if (!gstreamerAppSrc.encoder) {
		g_printerr ("Encoder element could not be created. Exiting.\n");
		return -1;
	}
	if (!gstreamerAppSrc.filter2) {
		g_printerr ("Filter 2 element could not be created. Exiting.\n");
		return -1;
	}
	if (!gstreamerAppSrc.parser) {
		g_printerr ("Parser element could not be created. Exiting.\n");
		return -1;
	}
	if (!gstreamerAppSrc.muxer) {
		g_printerr ("Muxer element could not be created. Exiting.\n");
		return -1;
	}
	if (!gstreamerAppSrc.sink) {
		g_printerr ("Sink element could not be created. Exiting.\n");
		return -1;
	}
	if (!gstreamerAppSrc.filter2_caps) {
		g_printerr ("Filter 2 caps could not be created. Exiting.\n");
		return -1;
	}

	/* Set up elements */
	g_object_set (G_OBJECT (gstreamerAppSrc.filter2), "caps", gstreamerAppSrc.filter2_caps, NULL);
	gst_caps_unref (gstreamerAppSrc.filter2_caps);
//	g_object_set (G_OBJECT (source), "num-buffers", NUM_FRAMES_TO_CAPTURE, NULL);
	g_object_set (G_OBJECT (gstreamerAppSrc.source), "num-buffers", co.nFramesToCapture, NULL);

	/*int target_bitrate = atoi(bitrate_arg);

	if (target_bitrate < 100000) {
		printf("Bitrate too low! setting to 100000\n");
		target_bitrate = 100000;
	} else if (target_bitrate > 100000000) {
		printf("Bitrate is ridiculous!  Setting to 100 Mbps\n");
		target_bitrate = 100000000;
	}*/

	//! Settings for the h264 encoder:
	//g_object_set (G_OBJECT (gstreamerAppSrc.encoder), "bitrate", co.bitRate, NULL);
	g_object_set (G_OBJECT (gstreamerAppSrc.encoder), "target-bitrate", co.bitRate, NULL);
	//g_object_set (G_OBJECT (gstreamerAppSrc.encoder), "iframeinterval", co.iframeinterval, NULL);
	g_object_set (G_OBJECT (gstreamerAppSrc.encoder), "control-rate", 0, NULL);

	// Fixing intermittentcorrupted file issue
	g_object_set(G_OBJECT (gstreamerAppSrc.muxer), "faststart", TRUE, NULL);
	g_object_set(G_OBJECT (gstreamerAppSrc.muxer), "fragment-duration", 500, NULL);




	return 0;
}

int GstreamerAppSrc::pipeline_make()
{

#if 1

	/*
	gst-launch-1.0 videotestsrc ! \
		'video/x-raw, format=(string)GRAY8, width=(int)1280, height=(int)720' ! \
		videoconvert ! \
		omxh264enc bitrate=10000000 ! \
		'video/x-h264, stream-format=(string)byte-stream' ! \
		h264parse ! \
		qtmux ! \
		filesink location=test.mp4 -e
	 */

	gst_bin_add_many(GST_BIN (gstreamerAppSrc.pipeline),
		gstreamerAppSrc.source,
		gstreamerAppSrc.filter1,
		gstreamerAppSrc.convert,
#if USE_MONOCHROME_CAPTURE
		gstreamerAppSrc.convFilter,
#endif
		gstreamerAppSrc.queue,
		gstreamerAppSrc.encoder,
		gstreamerAppSrc.filter2,
		gstreamerAppSrc.parser,
		gstreamerAppSrc.muxer,
		gstreamerAppSrc.sink, NULL);

	gst_element_link_many(
		gstreamerAppSrc.source,
		gstreamerAppSrc.filter1,
		gstreamerAppSrc.convert,
#if USE_MONOCHROME_CAPTURE
		gstreamerAppSrc.convFilter,
#endif
		gstreamerAppSrc.queue,
		gstreamerAppSrc.encoder,
		gstreamerAppSrc.filter2,
		gstreamerAppSrc.parser,
		gstreamerAppSrc.muxer,
		gstreamerAppSrc.sink, NULL);
	//
    //       gstreamerAppSrc.sink, NULL);

#else
	if (co.saveVideo) {
		gst_bin_add_many(GST_BIN (gstreamerAppSrc.pipeline), gstreamerAppSrc.source, gstreamerAppSrc.filter1, gstreamerAppSrc.convert,
			gstreamerAppSrc.encoder, gstreamerAppSrc.filter2, gstreamerAppSrc.parser, gstreamerAppSrc.queue, gstreamerAppSrc.muxer, gstreamerAppSrc.sink, NULL);
		gst_element_link_many(gstreamerAppSrc.source, gstreamerAppSrc.filter1, gstreamerAppSrc.convert, gstreamerAppSrc.encoder,
            gstreamerAppSrc.filter2, gstreamerAppSrc.parser, gstreamerAppSrc.queue, gstreamerAppSrc.muxer, gstreamerAppSrc.sink, NULL);
	} else {
		/* we add all elements into the pipeline */
		gst_bin_add_many( GST_BIN (gstreamerAppSrc.pipeline), gstreamerAppSrc.source, gstreamerAppSrc.filter1, gstreamerAppSrc.convert, gstreamerAppSrc.sink, NULL);
		/* we link the elements together */
		gst_element_link_many( gstreamerAppSrc.source, gstreamerAppSrc.filter1, gstreamerAppSrc.convert, gstreamerAppSrc.sink, NULL);
	}
#endif

	//! To get verbose output (needed for setting up video streaming)
	//g_signal_connect( gstreamerAppSrc.pipeline, "deep-notify", G_CALLBACK( gst_object_default_deep_notify ), NULL );

	return 0;
}

int GstreamerAppSrc::watcher_make()
{
	/* we add a message handler */
	bus = gst_pipeline_get_bus (GST_PIPELINE (gstreamerAppSrc.pipeline));
	gstreamerAppSrc.bus_watch_id = gst_bus_add_watch (bus, GstreamerAppSrc::bus_call, loop);
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
                po::value<std::string>(&(co.output))->default_value("videoFile.mp4"),
                "output file name, ex: vga_output1.vga")
//            ("encoder,e",
//                po::value<std::string>(&(co.encoder))->default_value("h264"),
//                "encoder type: h264, mpeg4, vp8")
			("saveVideo",
				po::value<int>(&co.saveVideo)->default_value(1),
				"save video")
			("enableDisplay",
				po::value<int>(&co.enableDisplay)->default_value(0),
				"enable display")
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
                po::value<int>(&(co.bitRate))->default_value(8000000),
                "Bit Rate (Integer), ex: 8000000")
            ("iframeinterval",
				po::value<int>(&(co.iframeinterval))->default_value(4),
				"H264 encoder intraframe interval (keyframe interval)")
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

	if (!gstreamerAppSrc.OpenCamera(co.frameRate))
	{
	  return -1;
	}

	/* init GStreamer */
	gst_init (&argc, &argv);

	loop = g_main_loop_new (NULL, FALSE);

	//! Try one more similar to RidgeRun's method:
	//! This pipeline works well.  Now, to hook it up to our appsrc:

	/* Check input arguments */
//	if (argc < 3) {
//		g_printerr ("Usage: %s <Recorded file name eg: test.h264> target_bitrate\n", argv[0]);
//		return -1;
//	}

	char const* output_location = co.output.c_str();
	printf("\n\n out file: %s\n\n\n", output_location);
	if(gstreamerAppSrc.factory_make(output_location) != 0)
 		return -1;
//	if(factory_make(argv[1]) != 0)
//		return -1;

	/* Add function to watch bus */
	if(gstreamerAppSrc.watcher_make() != 0)
		return -1;

	/* Add elements to pipeline, and link them */
	if(gstreamerAppSrc.pipeline_make() != 0)
		return -1;

	/* play */
	gst_element_set_state (gstreamerAppSrc.pipeline, GST_STATE_PLAYING);

	g_main_loop_run(loop);

	/* clean up */
	gst_element_set_state (gstreamerAppSrc.pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (gstreamerAppSrc.pipeline));
	g_main_loop_unref (loop);

	//! Close log file:
	if (co.saveMetadata) {
		fprintf(timeStampFile, "];\n");
        fclose(timeStampFile);
        timeStampFile = NULL;
	}

	return 0;
}
