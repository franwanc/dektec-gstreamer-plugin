/*
 * GStreamer
 * Copyright (C) 2014 Frank Wang <<franwanc@gmail.com>>
 * 
 */

/**
 * SECTION:element-dta2152src
 *
 * FIXME:Describe dta2152src here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v dta2152src ! xvimagesink sync=false
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>

#include <DTAPI.h>
#include "gstdta2152src.h"

GST_DEBUG_CATEGORY_STATIC (gst_dta2152_src_debug);
#define GST_CAT_DEFAULT gst_dta2152_src_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_MODE,
  PROP_CONNECTION,
  PROP_PORT_NUMBER
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("videosrc",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_dta2152_src_parent_class parent_class
G_DEFINE_TYPE (GstDta2152Src, gst_dta2152_src, GST_TYPE_ELEMENT);

static void gst_dta2152_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dta2152_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstStateChangeReturn
gst_dta2152_src_change_state (GstElement * element, GstStateChange transition);

static void gst_dta2152_src_task (void *priv);

//	Globals
const int c_BufSize = 16*1024*1024;     // Data transfer buffer size
unsigned char buff_i[c_BufSize];
unsigned char buff_o[c_BufSize];

/*************************************
*** helpers start ********************/

int ext_is_device_open()
{
	return 0;  // stub for now...	
}

GType
gst_dta2152_mode_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue modes[] = {
    {GST_DTA2152_MODE_NTSC, "ntsc", "NTSC SD 60i"},
    {GST_DTA2152_MODE_PAL, "pal", "PAL SD 50i"},
    {GST_DTA2152_MODE_1080i5994, "1080i5994", "HD1080 59.94i"},
    {GST_DTA2152_MODE_1080i50, "1080i50", "HD1080 50i"},
    {GST_DTA2152_MODE_720p5994, "720p5994", "HD720 59.94p"},
    {GST_DTA2152_MODE_720p50, "720p50", "HD720 50p"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstDta2152Modes", modes);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

GType
gst_dta2152_connection_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue connections[] = {
    {GST_DTA2152_CONNECTION_SDI, "sdi", "SDI"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstDta2152Connection", connections);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

#define true  1
#define false 0

#define NTSC 10, 11, false, false
#define PAL 12, 11, true, false
#define HD 1, 1, false, true

static const GstDta2152Mode modes[] = {

  /* ACTIVE VIDEO params */
  //{DTAPI_VIDSTD_525I59_94, 720, 486, 30000, 1001, true, NTSC},
  //{DTAPI_VIDSTD_625I50, 720, 576, 25, 1, true, PAL},
  //{DTAPI_VIDSTD_1080I59_94, 1920, 1080, 60000, 1001, true, HD},
  //{DTAPI_VIDSTD_1080I50, 1920, 1080, 25, 1, true, HD},
  //{DTAPI_VIDSTD_720P59_94, 1280, 720, 60000, 1001, false, HD},
  //{DTAPI_VIDSTD_720P50, 1280, 720, 50, 1, false, HD}

  /* FULL SDI FRAME params (including VANC and HANC)
     <ref> Table7 in following doc:
     http://www.appliedelectronics.com/documents/Guide%20to%20Standard%20HD%20Digital%20Video%20Measurements.pdf
  */   
  {DTAPI_VIDSTD_525I59_94, 858, 525, 30000, 1001, true, NTSC},
  {DTAPI_VIDSTD_625I50, 864, 625, 25, 1, true, PAL}, 
  {DTAPI_VIDSTD_1080I59_94, 2200, 1125, 60000, 1001, true, HD},
  {DTAPI_VIDSTD_1080I50, 2640, 1125, 25, 1, true, HD},
  {DTAPI_VIDSTD_720P59_94, 1650, 750, 60000, 1001, false, HD},
  {DTAPI_VIDSTD_720P50, 1980, 750, 50, 1, false, HD}

};

const GstDta2152Mode *
gst_dta2152_get_mode (GstDta2152ModeEnum e)
{
  return &modes[e];
}

static GstStructure *
gst_dta2152_mode_get_structure (GstDta2152ModeEnum e)
{
  const GstDta2152Mode *mode = &modes[e];

  return gst_structure_new ("video/x-raw",
      "format", G_TYPE_STRING, "UYVY",
      "width", G_TYPE_INT, mode->width,
      "height", G_TYPE_INT, mode->height,
      "framerate", GST_TYPE_FRACTION, mode->fps_n, mode->fps_d,
      "interlace-mode", G_TYPE_STRING, mode->interlaced ? "interleaved" : "progressive",
      "pixel-aspect-ratio", GST_TYPE_FRACTION, mode->par_n, mode->par_d,
      "colorimetry", G_TYPE_STRING, mode->is_hdtv ? "bt709" : "bt601",
      "chroma-site", G_TYPE_STRING, "mpeg2", NULL);
}

GstCaps *
gst_dta2152_mode_get_caps (GstDta2152ModeEnum e)
{
  GstCaps *caps;

  caps = gst_caps_new_empty ();
  gst_caps_append_structure (caps, gst_dta2152_mode_get_structure (e));

  return caps;
}

GstCaps *
gst_dta2152_mode_get_template_caps (void)
{
  int i;
  GstCaps *caps;
  GstStructure *s;

  caps = gst_caps_new_empty ();
  for (i = 0; i < (int) G_N_ELEMENTS (modes); i++) {
    s = gst_dta2152_mode_get_structure ((GstDta2152ModeEnum) i);
    gst_caps_append_structure (caps, s);
  }
  
  return caps;
}

static gboolean
gst_dta2152_src_video_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstDta2152Src *dta2152src;
  gboolean ret = FALSE;

  dta2152src = GST_DTA2152SRC (parent);

  GST_DEBUG_OBJECT (pad, "query: %" GST_PTR_FORMAT, query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      GstClockTime min_latency, max_latency;
      const GstDta2152Mode *mode;

      /* device must be open */
      if ( !ext_is_device_open() )   
      {
        GST_WARNING_OBJECT (dta2152src, "Can't give latency since device isn't open !");
        goto done;
      }

      mode = gst_dta2152_get_mode (dta2152src->mode);

      /* min latency is the time to capture one frame */
      min_latency =
          gst_util_uint64_scale_int (GST_SECOND, mode->fps_d, mode->fps_n);

      /* max latency is total duration of the frame buffer */
      max_latency = 2 * min_latency;

      GST_DEBUG_OBJECT (dta2152src,
          "report latency min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
          GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

      /* we are always live, the min latency is 1 frame and the max latency is
       * the complete buffer of frames. */
      gst_query_set_latency (query, TRUE, min_latency, max_latency);

      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

done: 
  return ret;
}

/* events sent to this element directly, mainly from the application */
static gboolean
gst_dta2152_src_send_event (GstElement * element, GstEvent * event)
{
  GstDta2152Src *src;
  gboolean result = FALSE;

  src = GST_DTA2152SRC (element);

  GST_DEBUG_OBJECT (src, "handling event %p %" GST_PTR_FORMAT, event, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      g_atomic_int_set (&src->pending_eos, TRUE);
      GST_INFO_OBJECT (src, "EOS pending");
      //g_cond_signal (&src->cond);
      result = TRUE;
      break;
      break;
    case GST_EVENT_TAG:
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_BOTH:
      /* Insert TAG, CUSTOM_DOWNSTREAM, CUSTOM_BOTH in the dataflow */
      GST_OBJECT_LOCK (src);
      src->pending_events = g_list_append (src->pending_events, event);
      g_atomic_int_set (&src->have_events, TRUE);
      GST_OBJECT_UNLOCK (src);
      event = NULL;
      result = TRUE;
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    case GST_EVENT_CUSTOM_BOTH_OOB:
      /* insert a random custom event into the pipeline */
      GST_DEBUG_OBJECT (src, "pushing custom OOB event downstream");
      result = gst_pad_push_event (src->videosrcpad, gst_event_ref (event));
      /* we gave away the ref to the event in the push */
      event = NULL;
      break;
    case GST_EVENT_CUSTOM_UPSTREAM:
      /* drop */
    case GST_EVENT_SEGMENT:
      /* sending random SEGMENT downstream can break sync - drop */
    default:
      GST_LOG_OBJECT (src, "dropping %s event", GST_EVENT_TYPE_NAME (event));
      break;
  }

  /* if we still have a ref to the event, unref it now */
  if (event)
    gst_event_unref (event);

  return result;
}

/**** helpers end ********************
**************************************/


/* GObject vmethod implementations */

/* initialize the dta2152src's class */
static void
gst_dta2152_src_class_init (GstDta2152SrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_dta2152_src_set_property;
  gobject_class->get_property = gst_dta2152_src_get_property;
  gstelement_class->send_event = GST_DEBUG_FUNCPTR (gst_dta2152_src_send_event);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_dta2152_src_change_state);

  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode", "Mode", "Video mode to use for capture",
          GST_TYPE_DTA2152_MODE, GST_DTA2152_MODE_NTSC,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_CONNECTION,
      g_param_spec_enum ("connection", "Connection",
          "Video input connection to use",
          GST_TYPE_DTA2152_CONNECTION, GST_DTA2152_CONNECTION_SDI,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_PORT_NUMBER,
      g_param_spec_int ("port-number", "Port number",
          "Capture port instance to use", 1, 2 /* max value */, 1,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  gst_element_class_set_details_simple(gstelement_class,
    "DTA2152 source",
    "Source/Video",
    "DTA2152 Video Source Element",
    "Frank Wang <<frankw0408@gmail.com>>");
     
  gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new ("videosrc", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_dta2152_mode_get_template_caps ()));
      
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_dta2152_src_init (GstDta2152Src * filter)
{
  GstDta2152SrcClass *dta2152src_class;
  dta2152src_class = GST_DTA2152SRC_GET_CLASS (filter);

  g_rec_mutex_init (&filter->task_mutex);
  filter->task = gst_task_new (gst_dta2152_src_task, filter, NULL);
  gst_task_set_lock (filter->task, &filter->task_mutex);

  filter->videosrcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (GST_ELEMENT_CLASS (dta2152src_class), "videosrc"), "videosrc");

  gst_pad_set_query_function (filter->videosrcpad, GST_DEBUG_FUNCPTR (gst_dta2152_src_video_src_query));
  gst_element_add_pad (GST_ELEMENT (filter), filter->videosrcpad);

  GST_OBJECT_FLAG_SET (filter, GST_ELEMENT_FLAG_SOURCE);

  filter->mode = GST_DTA2152_MODE_NTSC;
  filter->connection = GST_DTA2152_CONNECTION_SDI;
  filter->port_number = 1;

  filter->frame_num = -1;  /* -1 so will be 0 after incrementing */

}

static void
gst_dta2152_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDta2152Src *filter = GST_DTA2152SRC (object);

  switch (prop_id) {
    case PROP_MODE:
      filter->mode = (GstDta2152ModeEnum) g_value_get_enum (value);
      break;
    case PROP_CONNECTION:
      filter->connection = (GstDta2152ConnectionEnum) g_value_get_enum (value);
      break;
    case PROP_PORT_NUMBER:
      filter->port_number = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dta2152_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDta2152Src *filter = GST_DTA2152SRC (object);

  switch (prop_id) {
    case PROP_MODE:
      g_value_set_enum (value, filter->mode);
      break;
    case PROP_CONNECTION:
      g_value_set_enum (value, filter->connection);
      break;
    case PROP_PORT_NUMBER:
      g_value_set_int (value, filter->port_number);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

static gboolean
gst_dta2152_src_start (GstElement * element)
{
	g_print ("device start...in gst_dta2152_src_start() \n");

    GstDta2152Src *dta2152src = GST_DTA2152SRC (element);
    const GstDta2152Mode *mode;
    mode = gst_dta2152_get_mode (dta2152src->mode);
	
	// dt port init
	int port = dta2152src->port_number;		// port range 1..2

    DTAPI_RESULT dr;
    int VidStd=-1, SdiSubValue=-1;  //DTAPI_VIDSTD_UNKNOWN;
    int usrVidStd=-1;
    int FifoLoad, NumToRead;

	// attach to device
	dta2152src->m_pDvc = new DtDevice; 
	if (dta2152src->m_pDvc->AttachToType(2152) != DTAPI_OK) 
	{ 
		printf("No DTA-2152 in system ...\n"); 
		return FALSE; 
	}
	printf("found DTA-2152 in system.\n"); 

	// set port to input
    dr = dta2152src->m_pDvc->SetIoConfig(port, DTAPI_IOCONFIG_IODIR, DTAPI_IOCONFIG_INPUT, DTAPI_IOCONFIG_INPUT);
    if ( dr != DTAPI_OK )
    {
    	printf("failed to configure port %d to input port\n", port);
    	return FALSE;
    }
    printf("ocnfigured port %d to input port\n", port );
		
	// attach to input channel
	dta2152src->m_pSdiIn = new DtInpChannel; 
	if (dta2152src->m_pSdiIn->AttachToPort(dta2152src->m_pDvc, port) != DTAPI_OK) 
	{ 
		printf("Can't attach to input channel %d....\n", port); 
		return FALSE; 
	}
	printf("attached to DTA-2152 port %d.\n", port); 				

    // Get signal status (including detected video standard)
    dr = dta2152src->m_pSdiIn->DetectIoStd(VidStd, SdiSubValue);
    if (dr!=DTAPI_OK && dr!=DTAPI_E_INVALID_VIDSTD && dr!=DTAPI_E_NO_VIDSTD)
    {
    	printf("failed to detect input standard\n");
    	return FALSE;
    }
    printf("input video format detected %s\n", DtapiVidStd2Str(SdiSubValue) );
	
	int fmt = mode->mode;
	
    // Set user IO config - configure video format
    if ( fmt == DTAPI_VIDSTD_525I59_94 || fmt == DTAPI_VIDSTD_625I50 )
	{
		// SD video format
		usrVidStd = DTAPI_IOCONFIG_SDI;
	}
	else
	{
		// HD video format
		usrVidStd = DTAPI_IOCONFIG_HDSDI;
	}
			
    dr = dta2152src->m_pSdiIn->SetIoConfig(DTAPI_IOCONFIG_IOSTD, usrVidStd, fmt);
    if (dr != DTAPI_OK)
    {
    	printf("failed to configure video format - %s\n", DtapiResult2Str(dr));
    	return FALSE;
    }
	printf("input format configured okay to %s\n", DtapiVidStd2Str(fmt) );    	

    // Set the receive mode
    dr = dta2152src->m_pSdiIn->SetRxMode(DTAPI_RXMODE_SDI_FULL);
    if ( dr != DTAPI_OK )
    {	
        printf( "failed to set reception mode - %s\n", DtapiResult2Str(dr) );
        return FALSE;
	}
	printf( "configured rx mode...\n");

    // Start reception
    dr = dta2152src->m_pSdiIn->SetRxControl(DTAPI_RXCTRL_RCV);
    if ( dr != DTAPI_OK )
    {	
        printf( "failed to start reception - %s\n", DtapiResult2Str(dr) );
        return FALSE;
	}
	printf( "started reception...\n");
		
    g_rec_mutex_lock (&dta2152src->task_mutex);
    gst_task_start (dta2152src->task);
    g_rec_mutex_unlock (&dta2152src->task_mutex);
		
	return TRUE;
}

static gboolean
gst_dta2152_src_stop (GstElement * element)
{
  g_print ("device stop...\n");

  GstDta2152Src *dta2152src = GST_DTA2152SRC (element);

  gst_task_stop (dta2152src->task);

  dta2152src->stop = TRUE;

  gst_task_join (dta2152src->task);

  g_list_free_full (dta2152src->pending_events,
      (GDestroyNotify) gst_mini_object_unref);
  dta2152src->pending_events = NULL;
  dta2152src->have_events = FALSE;
  dta2152src->pending_eos = FALSE;

  return TRUE;
}

static void
gst_dta2152_src_send_initial_events (GstDta2152Src * src)
{
  GstSegment segment;
  GstEvent *event;
  guint group_id;
  guint32 audio_id, video_id;
  gchar stream_id[9];

  /* stream-start */
  audio_id = g_random_int ();
  video_id = g_random_int ();
  while (video_id == audio_id)
    video_id = g_random_int ();

  g_snprintf (stream_id, sizeof (stream_id), "%08x", video_id);
  event = gst_event_new_stream_start (stream_id);
  gst_event_set_group_id (event, group_id);
  gst_pad_push_event (src->videosrcpad, event);

  /* caps */
  gst_pad_push_event (src->videosrcpad,
      gst_event_new_caps (gst_dta2152_mode_get_caps (src->mode)));

  /* segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  event = gst_event_new_segment (&segment);
  gst_pad_push_event (src->videosrcpad, gst_event_ref (event));

}

void gst_dta2152_src_task(void * priv)
{
	GstDta2152Src *dta2152src = GST_DTA2152SRC (priv);
	GstBuffer *buffer;
	GstBuffer *audio_buffer;
	void *data;
	gsize data_size;
	int n_samples;
	GstFlowReturn video_flow, audio_flow, flow;
	const GstDta2152Mode *mode;
	gboolean discont = FALSE;
	
    DTAPI_RESULT dr;
	int NumToRead;
	
    GST_DEBUG_OBJECT (dta2152src, "task");

    dta2152src->frame_num++;
    
	static int numFrames = 0;
	{
		// read SDI frame - 8bit FULL
		NumToRead = c_BufSize;
        dr = dta2152src->m_pSdiIn->ReadFrame((unsigned int*)&buff_i, NumToRead);
        numFrames ++;

		// live probe
		if ( (numFrames%100) == 0 )
		{
		    if (dr != DTAPI_OK)
		    {
		    	printf("failed to read video frames - %s\n", DtapiResult2Str(dr));
		    }
			else
				printf("read another 100 frames with frame size of %d \n", NumToRead);	
		}				
	}

	if (!dta2152src->started) 
	{
    	gst_dta2152_src_send_initial_events (dta2152src);
    	dta2152src->started = TRUE;
  	}

    if (g_atomic_int_get (&dta2152src->have_events)) 
    {
	    GList *l;
	
	    GST_OBJECT_LOCK (dta2152src);
	    for (l = dta2152src->pending_events; l != NULL; l = l->next) 
	    {
	      GstEvent *event = GST_EVENT (l->data);
	
	      GST_DEBUG_OBJECT (dta2152src, "pushing %s event",
	          GST_EVENT_TYPE_NAME (event));
	      gst_pad_push_event (dta2152src->videosrcpad, gst_event_ref (event));
	      l->data = NULL;
	    }
	    g_list_free (dta2152src->pending_events);
	    dta2152src->pending_events = NULL;
	    g_atomic_int_set (&dta2152src->have_events, FALSE);
	    GST_OBJECT_UNLOCK (dta2152src);
    }

    mode = gst_dta2152_get_mode (dta2152src->mode);

    data = buff_i;
    data_size = mode->width * mode->height * 2;

    buffer = gst_buffer_new_and_alloc (data_size);
    gst_buffer_fill (buffer, 0, data, data_size);

    GST_BUFFER_TIMESTAMP (buffer) =
      gst_util_uint64_scale_int (dta2152src->frame_num * GST_SECOND,
      mode->fps_d, mode->fps_n);
    GST_BUFFER_DURATION (buffer) =
      gst_util_uint64_scale_int ((dta2152src->frame_num + 1) * GST_SECOND,
      mode->fps_d, mode->fps_n) - GST_BUFFER_TIMESTAMP (buffer);
    GST_BUFFER_OFFSET (buffer) = dta2152src->frame_num;
    GST_BUFFER_OFFSET_END (buffer) = dta2152src->frame_num;

    if (dta2152src->frame_num == 0)
    	discont = TRUE;

    if (discont)
    	GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    else
    	GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DISCONT);

    video_flow = gst_pad_push (dta2152src->videosrcpad, buffer);

}

static GstStateChangeReturn
gst_dta2152_src_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  gboolean no_preroll = FALSE;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_dta2152_src_start (element)) {
        ret = GST_STATE_CHANGE_FAILURE;
        goto out;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      no_preroll = TRUE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      no_preroll = TRUE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_dta2152_src_stop (element);
      break;
    default:
      break;
  }

  if (no_preroll && ret == GST_STATE_CHANGE_SUCCESS)
    ret = GST_STATE_CHANGE_NO_PREROLL;

out:
  return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
dta2152src_init (GstPlugin * dta2152src)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template dta2152src' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_dta2152_src_debug, "dta2152src",
      0, "Template dta2152src");

  return gst_element_register (dta2152src, "dta2152src", GST_RANK_NONE,
      GST_TYPE_DTA2152SRC);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstdta2152src"
#endif

/* gstreamer looks for this structure to register dta2152srcs
 *
 * exchange the string 'Template dta2152src' with your dta2152src description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    dta2152src,
    "DTA2152 Video Source Element",
    dta2152src_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
