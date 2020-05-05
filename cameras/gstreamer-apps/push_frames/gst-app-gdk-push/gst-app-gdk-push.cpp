#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

GST_DEBUG_CATEGORY (appsrc_pipeline_debug);
#define GST_CAT_DEFAULT appsrc_pipeline_debug

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

typedef struct _App App;

struct _App
{
  GstElement *pipeline;
  GstElement *appsrc;

  GMainLoop *loop;
  guint sourceid;

  GTimer *timer;

};

App s_app;



static gboolean
read_data (App * app)
{
    guint len;
    GstFlowReturn ret;
    gdouble ms;
    
    static int count = 0;
    static double last_call_time = ms_time() - 1000;

    ms = g_timer_elapsed(app->timer, NULL);
    
    double elapsed = ms_time() - last_call_time;
    
    if (elapsed < 100.0) {
		//printf("Time elapsed: %lf\n", elapsed);
		return TRUE;
	}    
	
	GdkPixbuf *pb;
	gboolean ok = TRUE;

	//buffer = gst_buffer_new();

	pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 640, 480);
	gdk_pixbuf_fill(pb, 0xffffffff);
	
	//gst_buffer_map(buf, &info), then use info.data, and
	//gst_buffer_unmap(buf, &info) when you're done.
	
	int imageSizeBytes = 640*480*3*sizeof(guchar);;

	GstBuffer *buffer = gst_buffer_new_wrapped_full(
			(GstMemoryFlags)0,
			(gpointer)gdk_pixbuf_get_pixels(pb),
			imageSizeBytes,
			0,
			imageSizeBytes,
			NULL,
			NULL);

	GST_DEBUG ("feed buffer");
	g_signal_emit_by_name (app->appsrc, "push-buffer", buffer, &ret);
	gst_buffer_unref (buffer);

	if (ret != GST_FLOW_OK) {
		/* some error, stop sending data */
		GST_DEBUG ("some error");
		ok = FALSE;
	}	

	g_timer_start(app->timer);
	
	last_call_time = ms_time();
	
	printf("read_data called %d times\n", count++);
	//usleep(100000);

	return ok;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void
start_feed (GstElement * pipeline, guint size, App * app)
{
  if (app->sourceid == 0) {
    GST_DEBUG ("start feeding");
    app->sourceid = g_idle_add ((GSourceFunc) read_data, app);
  }
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void
stop_feed (GstElement * pipeline, App * app)
{
  if (app->sourceid != 0) {
    GST_DEBUG ("stop feeding");
    g_source_remove (app->sourceid);
    app->sourceid = 0;
  }
}

static gboolean
bus_message (GstBus * bus, GstMessage * message, App * app)
{
  GST_DEBUG ("got message %s",
      gst_message_type_get_name (GST_MESSAGE_TYPE (message)));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
        GError *err = NULL;
        gchar *dbg_info = NULL;

        gst_message_parse_error (message, &err, &dbg_info);
        g_printerr ("ERROR from element %s: %s\n",
            GST_OBJECT_NAME (message->src), err->message);
        g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
        g_error_free (err);
        g_free (dbg_info);
        g_main_loop_quit (app->loop);
        break;
    }
    case GST_MESSAGE_EOS:
      g_main_loop_quit (app->loop);
      break;
    default:
      break;
  }
  return TRUE;
}

int
main (int argc, char *argv[])
{
  App *app = &s_app;
  GError *error = NULL;
  GstBus *bus;
  GstCaps *caps;

  gst_init (&argc, &argv);

  GST_DEBUG_CATEGORY_INIT (appsrc_pipeline_debug, "appsrc-pipeline", 0,
      "appsrc pipeline example");

  /* create a mainloop to get messages and to handle the idle handler that will
   * feed data to appsrc. */
  app->loop = g_main_loop_new (NULL, TRUE);
  app->timer = g_timer_new();

  //app->pipeline = gst_parse_launch("appsrc name=mysource ! video/x-raw-rgb,width=640,height=480,bpp=24,depth=24 !"
//	" ffmpegcolorspace ! videoscale method=1 ! theoraenc bitrate=150 ! udpsink host=127.0.0.1 port=1234", NULL);

//  app->pipeline = gst_parse_launch("appsrc name=mysource ! video/x-raw-rgb,width=640,height=480,bpp=24,depth=24 ! "
//	"autovideosink", NULL);
	
  app->pipeline = gst_parse_launch("appsrc name=mysource ! queue ! autovideosink", NULL);

  g_assert (app->pipeline);

  bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));
  g_assert(bus);

  /* add watch for messages */
  gst_bus_add_watch (bus, (GstBusFunc) bus_message, app);

  /* get the appsrc */
    app->appsrc = gst_bin_get_by_name (GST_BIN(app->pipeline), "mysource");
    g_assert(app->appsrc);
    g_assert(GST_IS_APP_SRC(app->appsrc));
    //g_signal_connect (app->appsrc, "need-data", G_CALLBACK (start_feed), app);
    //g_signal_connect (app->appsrc, "enough-data", G_CALLBACK (stop_feed), app);


	//gstreamerAppSrc.source = gst_element_factory_make ("appsrc", "source");
	
	g_object_set (G_OBJECT (app->appsrc), "caps",
		gst_caps_new_simple ("video/x-raw",
					 "format", G_TYPE_STRING, "RGB",
					 "width", G_TYPE_INT, 640,
					 "height", G_TYPE_INT, 480,
					 NULL), 
				NULL);

	/* setup appsrc */
	g_object_set (G_OBJECT (app->appsrc),
		"stream-type", GST_APP_STREAM_TYPE_STREAM,
		"format", GST_FORMAT_TIME,
		"is-live", TRUE,
		"do-timestamp", TRUE,
		NULL);
	//g_signal_connect(gstreamerAppSrc.source, "need-data", G_CALLBACK (cb_need_data), &gstreamerAppSrc);



  /* set the caps on the source */ // OLD!!!!
  /*caps = gst_caps_new_simple ("video/x-raw-rgb",
    "bpp",G_TYPE_INT,24,
    "depth",G_TYPE_INT,24,
     "width", G_TYPE_INT, 640,
     "height", G_TYPE_INT, 480,
     NULL);
   gst_app_src_set_caps(GST_APP_SRC(app->appsrc), caps);
   * */
   
  //! Add a callback that will fill the buffers with data and push them:
  app->sourceid = g_idle_add ((GSourceFunc) read_data, app);


  /* go to playing and wait in a mainloop. */
  gst_element_set_state (app->pipeline, GST_STATE_PLAYING);

  /* this mainloop is stopped when we receive an error or EOS */
  g_main_loop_run (app->loop);

  GST_DEBUG ("stopping");

  gst_element_set_state (app->pipeline, GST_STATE_NULL);

  gst_object_unref (bus);
  g_main_loop_unref (app->loop);

  return 0;
}
