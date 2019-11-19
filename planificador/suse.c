#include "suse.h"
#include "network.h"
#include "semaphore.h"
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/node.h>
#include <commons/collections/queue.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#define SEM_ID_MAX_LENGTH 32


t_log *suse_logger = NULL;
suse_configuration *suse_config = NULL;

void suse_stop_service()
{
	int i;
	log_info(suse_logger,"Received SIGINT signal shuting down!");

	log_destroy(suse_logger);
	
	free(suse_config->sem_id[0]);
	free(suse_config->sem_id[1]);
	free(suse_config->sem_id);

	free(suse_config->sem_init[0]);
	free(suse_config->sem_init[1]);
	free(suse_config->sem_init);

	free(suse_config->sem_max[0]);
	free(suse_config->sem_max[1]);
	free(suse_config->sem_max);
	free(suse_config);

	suse_config = NULL;
	suse_logger = NULL;
	
	server_stop();
}

//TODO revisar si esto no puedo sacarlo del utils
/*
void *client_handler(void *args)
{
	ConnectionArgs *conna = (ConnectionArgs *)args;
	Message msg;
	char buffer[1024];
	int n=0;
	int sock = conna->client_fd;
	struct sockaddr_in client_address = conna->client_addr;
	
	printf("A client has connected!\n");
	while((n=receive_packet(sock,buffer,1024)) > 0)
	{
		if((n = message_decode(buffer,n,&msg)) > 0)
		{
			message_handler(&msg,sock);
		}
	}	

	log_debug(suse_logger,"The client was disconnected!");
	close(sock);
}
*/

void *report_metrics(void *args) {
	char *log_string;

	/* Thread metrics */

	/* Process metrics */

	/* Semaphore metrics */
		
	alarm(suse_config->metrics_timer);
}

suse_configuration *load_configuration(char *path)
{
	t_config *config = config_create(path);
	suse_configuration *sc = (suse_configuration *)malloc(sizeof(suse_configuration));

	if(config == NULL)
	{	
		log_error(suse_logger,"Configuration couldn't be loaded.Quitting program!");
		free(sc);
		exit(-1);
	}
	
	sc->listen_port = config_get_int_value(config,"LISTEN_PORT");
	sc->metrics_timer = config_get_int_value(config,"METRICS_TIMER");
	sc->max_multiprog = config_get_int_value(config,"MAX_MULTIPROG");
	sc->sem_id = config_get_array_value(config,"SEM_IDS");
	sc->sem_init = config_get_array_value(config,"SEM_INIT");
	sc->sem_max = config_get_array_value(config,"SEM_MAX");
	sc->alpha_sjf = config_get_int_value(config,"ALPHA_SJF"); 

	config_destroy(config);
	return sc;
}


//todo revisar esto
void message_handler(Message *m,int sock)
{
	switch(m->header.message_type)
	{
		case MESSAGE_STRING:
			log_debug(suse_logger,"Received -> %s",m->data);	
			break;
		case MESSAGE_CALL:
			log_debug(suse_logger,"Remote call received!");
			//deprecated rcp se usaba json, actualiza a actual serializacion
			//rpc_server_invoke(m->data,sock);
			message_free_data(m);
			break;
		case MESSAGE_NEW_ULT:
			log_debug(suse_logger,"New ult arrived in the ready queue!\n");
			//do_somth
			break;
		default:
			log_error(suse_logger,"Undefined message");
			break;
	}
	return;

}

int main(){

}
