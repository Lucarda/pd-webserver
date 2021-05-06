/* 
 * Copyright (c) 2021 Lucas Cordiviola 
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "inter.h"


	

t_class *webserver_class;


int lmain();




static void webserver_main(t_webserver *x, t_symbol *folder, t_float port) {

	if(x->started) {
		logpost(x,2,"Server already running.");
		return;
	}
	
	
	x->options[0] = "document_root";
	x->options[1] = ".";
	x->options[2] = "listening_ports";
	x->options[3] = "8080";	
	x->options[4] = "request_timeout_ms";
	x->options[5] = "10000";
	x->options[6] = "error_log_file";
	x->options[7] = "error.log";
	x->options[8] = "enable_auth_domain_check";
	x->options[9] = "no";
	x->options[10] = 0;

	char completefolder[MAXPDSTRING];	

	
	/* taken from iem's soundfile_info */
  
	if(folder->s_name[0] == '/')/*make complete path + folder*/
	{
		strcpy(completefolder, folder->s_name);
	}
	else if( (((folder->s_name[0] >= 'A')&&(folder->s_name[0] <= 'Z')) || ((folder->s_name[0] >= 'a')&&(folder->s_name[0] <= 'z'))) && (folder->s_name[1] == ':') && (folder->s_name[2] == '/') )
	{
		strcpy(completefolder, folder->s_name);
	}
	else
	{
		strcpy(completefolder, canvas_getdir(x->x_canvas)->s_name);
		strcat(completefolder, "/");
		strcat(completefolder, folder->s_name);
	}


	/* </soundfile_info> */
	
	x->options[1] = completefolder;
	int num = (int)port;
	char nnum[10];
	sprintf(nnum,"%d",num);
	x->options[3] = nnum;

	x->exitNow = 0;

	
	pthread_create(&x->tid, NULL, lmain, x);

}


static void webserver_free(t_webserver *x) {
	
	x->exitNow = 1;

	
}


static void *webserver_new(void)
{

  t_webserver *x = (t_webserver *)pd_new(webserver_class);

  x->x_canvas = canvas_getcurrent();
  x->started = 0;
	   
  return (void *)x;
}




void webserver_setup(void) {

  webserver_class = class_new(gensym("webserver"),      
			       (t_newmethod)webserver_new,
			       (t_method)webserver_free,                          
			       sizeof(t_webserver),       
			       CLASS_DEFAULT,				   
			       0);                        


  class_addmethod(webserver_class, (t_method)webserver_free, gensym("stop"), 0);

  class_addmethod(webserver_class, (t_method)webserver_main, gensym("start"), A_SYMBOL, A_FLOAT, 0);
}