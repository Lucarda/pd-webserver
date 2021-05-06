#include "m_pd.h"
#include <pthread.h>

typedef struct _webserver {
  t_object  x_obj;
  t_canvas  *x_canvas;
  pthread_t tid;
  char folder[MAXPDSTRING];
  int exitNow;
  const char *options[MAXPDSTRING];
  int started;
  } t_webserver;




