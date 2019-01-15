/*
 * GStreamer
 * Copyright (C) 2014 Frank Wang <<franwanc@gmail.com>>
 * 
 */

#ifndef __GST_DTA2152SRC_H__
#define __GST_DTA2152SRC_H__

#include <gst/gst.h>
#include <DTAPI.h>

typedef enum {
  GST_DTA2152_MODE_NTSC,
  GST_DTA2152_MODE_PAL,
  GST_DTA2152_MODE_1080i5994,
  GST_DTA2152_MODE_1080i50,
  GST_DTA2152_MODE_720p5994,
  GST_DTA2152_MODE_720p50
} GstDta2152ModeEnum;

#define GST_TYPE_DTA2152_MODE (gst_dta2152_mode_get_type ())
GType gst_dta2152_mode_get_type (void);

typedef enum {
  GST_DTA2152_CONNECTION_SDI
} GstDta2152ConnectionEnum;

#define GST_TYPE_DTA2152_CONNECTION (gst_dta2152_connection_get_type ())
GType gst_dta2152_connection_get_type (void);

typedef struct _GstDta2152Mode GstDta2152Mode;
struct _GstDta2152Mode {
  int mode;         // dta2152 video format macro, such as DTAPI_VIDSTD_xxx
  int width;
  int height;
  int fps_n;
  int fps_d;
  gboolean interlaced;
  int par_n;
  int par_d;
  gboolean tff;
  gboolean is_hdtv;
};

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_DTA2152SRC (gst_dta2152_src_get_type())
#define GST_DTA2152SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DTA2152SRC,GstDta2152Src))
#define GST_DTA2152SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DTA2152SRC,GstDta2152SrcClass))
#define GST_DTA2152SRC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DTA2152SRC, GstDta2152SrcClass))
#define GST_IS_DTA2152SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DTA2152SRC))
#define GST_IS_DTA2152SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DTA2152SRC))

typedef struct _GstDta2152Src      GstDta2152Src;
typedef struct _GstDta2152SrcClass GstDta2152SrcClass;

struct _GstDta2152Src
{
  GstElement element;

  gboolean  pending_eos;    /* ATOMIC */
  gboolean  have_events;    /* ATOMIC */
  GList    *pending_events; /* OBJECT_LOCK */

  GstPad *videosrcpad;

  guint64 frame_num;

  gboolean stop;
  /* so we send a stream-start, caps, and newsegment events before buffers */
  gboolean started;

  /* properties */
  GstDta2152ModeEnum mode;
  GstDta2152ConnectionEnum connection;
  int port_number;

  GstTask *task;
  GRecMutex task_mutex;
  
  /* input channel */
  DtInpChannel *m_pSdiIn;
  DtDevice     *m_pDvc;

};

struct _GstDta2152SrcClass 
{
  GstElementClass parent_class;
};

GType gst_dta2152_src_get_type (void);

G_END_DECLS

#endif /* __GST_DTA2152SRC_H__ */
