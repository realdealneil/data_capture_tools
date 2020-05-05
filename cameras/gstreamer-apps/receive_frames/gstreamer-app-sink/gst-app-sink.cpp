// example appsrc for gstreamer 1.0 with own mainloop & external buffers. based on example from gstreamer docs.
// public domain, 2015 by Florian Echtler <floe@butterbrot.org>. compile with:
// gcc --std=c99 -Wall $(pkg-config --cflags gstreamer-1.0) -o gst gst.c $(pkg-config --libs gstreamer-1.0) -lgstapp-1.0

#include "gst-app-sink.h"
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

#include <cuda_runtime.h>

#define use_nvcam

namespace po = boost::program_options;

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

	//g_print("Sending EOS signal to pipeline\n");
	//gst_element_send_event(this->pipeline, gst_event_new_eos());
	sigint_restore();
	//exit(0);
}

gboolean GstreamerAppSink::bus_call (GstBus *bus, GstMessage *msg, gpointer data)
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


cudaError_t printPat(void * ptr)
{
    cudaPointerAttributes pat;
    cudaError_t cuerr = cudaPointerGetAttributes(&pat, ptr);

    if (cuerr == cudaError_t::cudaSuccess)
    {
        //fprintf(stdout, "Memory[%u] MapInfo data Attributes =\n"
        fprintf(stdout, "\tptr = %p, dev = %d, devPtr = %p, hostPtr = %p, isMan = %d, type = %d\n",
            ptr, pat.device, pat.devicePointer, pat.hostPointer, pat.memoryType);
    }
    else
    {
        fprintf(stderr, "\tcudaPointerGetAttributes returned %d\n", cuerr);
    }

    return cuerr;
}

int numBufsRxed = 0;
static GstFlowReturn on_new_sample (GstElement *sink, GstreamerAppSink *data)
{
    GstSample *sample;
    GstBuffer *buffer;
    printf("on_new_sample!!!\n");

    /* Retrive the buffer */
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    //sample = gst_app_sink_pull_sample( GST_APP_SINK (sink));
    //buffer = gst_sample_get_buffer (sample);

    if (sample)
    {
        /* Print a * to indicate a received buffer */
        numBufsRxed++;
        printf("*", numBufsRxed);
        if (numBufsRxed %20 == 0)
            printf("\n");
        //gst_buffer_unref (buffer);

        // from sample
        buffer = gst_sample_get_buffer(sample);
        auto * sinfo = gst_sample_get_info(sample);
        auto * scaps = gst_sample_get_caps(sample);

        //fprintf(stdout,"Sample GType = %s\n", g_type_name(gst_sample_get_type(sample)));

        // from sample >>> from info
        if (sinfo)
        {
            fprintf(stdout,"Sample info GType = %s\n", g_type_name(sinfo->type));
        }


        // from sample >>> from buffer
        //gst_meta_get_info()
        fprintf(stdout,"Buffer offset = %u\n", buffer->offset);
        fprintf(stdout,"Buffer offset_end = %u\n", buffer->offset_end);

        //auto * bmeta = gst_buffer_get_meta(buffer);
        gpointer itstate = nullptr;
        GstMeta * meta = gst_buffer_iterate_meta(buffer, &itstate);
        int nmeta = 0;
        while(meta)
        {
            fprintf(stdout, "Meta[%d] info Gtype = %s\n", nmeta, g_type_name(meta->info->type));
            fprintf(stdout, "Meta[%d] info api Gtype = %s\n", nmeta, g_type_name(meta->info->api));
            fprintf(stdout, "Meta[%d] info size = %u\n", nmeta, g_type_name(meta->info->size));
            nmeta++;
            meta = gst_buffer_iterate_meta(buffer, &itstate);
        }

        auto bnmem = gst_buffer_n_memory(buffer);
        fprintf(stdout,"Buffer n_memory = %u\n", bnmem);


        // from sample >>> from buffer >> from mem
        for (decltype(bnmem) nidx = 0; nidx < bnmem; nidx++)
        {
            GstMemory * mem = nullptr;
            mem = gst_buffer_peek_memory(buffer, nidx);

            if (mem)
            {
                //fprintf(stdout,"Memory[%u] Flags GType = %s\n",
                //    g_type_name(gst_memory_flags_get_type(mem)));

                gsize msz, moff, mmax;
                msz = gst_memory_get_sizes(mem, &moff, &mmax);
                fprintf(stdout, "Memory[%u] size = %u, offset = %u, maxsize = %u\n",
                    nidx, msz, moff, mmax);

                GstMapInfo mmapinfo;
                if (gst_memory_map(mem, &mmapinfo, GstMapFlags::GST_MAP_READ))
                {
                    fprintf(stdout, "Memory[%u] MapInfo data = %p\n",
                        nidx, mmapinfo.data);

                    fprintf(stdout, "Memory[%u] MapInfo data Attributes =\n", nidx);
                    printPat(mmapinfo.data);

                    fprintf(stdout, "Memory[%u] MapInfo data[buffer->offset] Attributes =\n", nidx);
                    printPat(*reinterpret_cast<void**>(&mmapinfo.data[buffer->offset]));


                }
                else
                {
                    fprintf(stdout, "Memory[%u] could not be read mapped\n", nidx);
                }
            }
            else
            {
                fprintf(stdout, "Memory[%u] is NULL\n", nidx);
            }

            fprintf(stdout, "Copying buffer...\n"); fflush(stdout);
            GstBuffer * new_buffer = gst_buffer_copy(buffer);

            if (new_buffer)
            {
                fprintf(stdout, "copy complete\nCoping into...\n"); fflush(stdout);
                if (gst_buffer_copy_into(new_buffer, buffer, GST_BUFFER_COPY_DEEP, 0, -1))
                {
                    fprintf(stdout, "copy complete\n");fflush(stdout);
                }
                else
                {
                    fprintf(stderr, "Could not copy into\n");
                }
            }
            else
            {
                fprintf(stderr, "Could not copy\n");
            }

            //gst_memory_get_type()
            //if ()
        }


        gst_sample_unref (sample);
    } else
    {
        printf("We didn't get another sample.  Last one we got was %d\n", numBufsRxed);
    }

    return GST_FLOW_OK;
}

#define USE_VISUAL_SINK 0

int GstreamerAppSink::factory_make()
{

    this->pipeline = gst_pipeline_new("sink_pipeline");

    //this->source = gst_element_factory_make("videotestsrc", "src");
    //this->sink = gst_element_factory_make("nveglglessink", "sink");

#ifdef use_nvcam
//gst-launch-1.0 nvcamerasrc ! 'video/x-raw(memory:NVMM), width=1280, height=960, format=(string)I420, framerate=(fraction)30/1' ! nvoverlaysink -e

	this->source = gst_element_factory_make("nvcamerasrc", "source");
	g_object_set(G_OBJECT (this->source),
				"sensor-id", 0, NULL);

	//! filter1: "application/x-rtp"
	this->filter1  = gst_element_factory_make ("capsfilter",	"filter1");
	this->filter1_caps = gst_caps_from_string(
        "video/x-raw(memory:NVMM), width=1280, height=960, format=(string)I420, framerate=(fraction)30/1");
//	this->filter1_caps = gst_caps_new_simple ("video/x-raw(memory:NVMM)",
//          "format", G_TYPE_STRING, "NV12",
//          "width", G_TYPE_INT, 1280,
//          "height", G_TYPE_INT, 720,
//          "framerate", GST_TYPE_FRACTION, 30, 1,
//          NULL);

    if (!this->filter1_caps) {
		g_printerr ("Filter1 caps could not be created. Exiting.\n");
		return -1;
	}
	g_object_set (G_OBJECT (this->filter1), "caps", this->filter1_caps, NULL);
	gst_caps_unref (this->filter1_caps);


#else
    //! SOURCE: udpsrc port=5004
    this->source = gst_element_factory_make("udpsrc", "source");
    g_object_set(G_OBJECT (this->source), "port", 5004, NULL);


    //! filter1: "application/x-rtp"
	this->filter1  = gst_element_factory_make ("capsfilter",	"filter1");
	this->filter1_caps = gst_caps_new_simple ("application/x-rtp", NULL);
    if (!this->filter1_caps) {
		g_printerr ("Filter1 caps could not be created. Exiting.\n");
		return -1;
	}
	g_object_set (G_OBJECT (this->filter1), "caps", this->filter1_caps, NULL);
	gst_caps_unref (this->filter1_caps);

	//! rtph264depay (depayload)
    this->depay = gst_element_factory_make("rtph264depay", "depay");

    //! h264parse
    this->parse = gst_element_factory_make("h264parse", "parse");

    //! omxh264dec
    this->decode = gst_element_factory_make("omxh264dec", "decode");
#endif // use_nvcam








    //! Sink: nveglglessink
#if USE_VISUAL_SINK
    //this->sink = gst_element_factory_make("nveglglessink", "sink");
    this->sink = gst_element_factory_make("nvoverlaysink", "sink");
#else
    this->sink = gst_element_factory_make("appsink", "app_sink");
    g_object_set (G_OBJECT (this->sink), "emit-signals", TRUE, NULL); //"sync", FALSE, NULL);
    g_signal_connect(G_OBJECT(this->sink), "new-sample", G_CALLBACK (on_new_sample), &gstreamerAppSink);
    //gst_object_unref (this->sink);
#endif

	return 0;
}

int GstreamerAppSink::pipeline_make()
{
    gst_bin_add_many(GST_BIN (this->pipeline),
        this->source,
        this->filter1,
#ifndef use_nvcam
        this->depay,
        this->parse,
        this->decode,
#endif // use_nvcam
        this->sink,
        NULL);

    gst_element_link_many(
		this->source,
        this->filter1,
#ifndef use_nvcam
        this->depay,
        this->parse,
        this->decode,
#endif // use_nvcam
		this->sink,
		NULL);

	return 0;
}

int GstreamerAppSink::watcher_make()
{
	/* we add a message handler */
/*
	bus = gst_pipeline_get_bus (GST_PIPELINE (this->pipeline));
	this->bus_watch_id = gst_bus_add_watch (bus, gstreamerAppSink::bus_call, loop);
	gst_object_unref (bus);
*/
	return 0;
}

gint main (gint argc, gchar *argv[])
{
	signal(SIGINT, intHandler);

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
				po::value<int>(&co.enableDisplay)->default_value(1),
				"enable display")
            ("width,w",
                po::value<int>(&(co.iWidth))->default_value(1280),
                "Image capture width (pixels)")
            ("height,h",
                po::value<int>(&(co.iHeight))->default_value(960),
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

	//char const* output_location = co.output.c_str();
	//printf("\n\n out file: %s\n\n\n", output_location);
	if(gstreamerAppSink.factory_make() != 0)
 		return -1;
//	if(factory_make(argv[1]) != 0)
//		return -1;

	/* Add function to watch bus */
	//if(this->watcher_make() != 0)
	//	return -1;

	/* Add elements to pipeline, and link them */
	if(gstreamerAppSink.pipeline_make() != 0)
		return -1;

	/* play */
	gst_element_set_state (gstreamerAppSink.pipeline, GST_STATE_PLAYING);

	g_main_loop_run(loop);

	/* clean up */
	gst_element_set_state (gstreamerAppSink.pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (gstreamerAppSink.pipeline));
	g_main_loop_unref (loop);

	return 0;
}
