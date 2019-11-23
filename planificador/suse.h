#ifndef SUSE_H
#define SUSE_H

#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <signal.h>
#include <utils_suse/libraries.h>
#include <semaphore.h>
#include <pthread.h>

#define SUSE_CONFIG_PATH "../configs/planificador.config"

typedef struct suse_configuration
{
	char* LISTEN_PORT;
	int METRICS_TIMER;
	char ** SEM_ID;
	char ** SEM_INIT;
	char ** SEM_MAX;
	int ALPHA_SJF;
	int MAX_MULTIPROG;
	t_list * process; //Hay que mantener un registro de los programas que tenemos.
	t_list * semaforos;
	uint32_t ACTUAL_MULTIPROG;
}suse_configuration;

typedef struct t_process
{
	t_list * ULTS; //Lista de t_suse_thread
	int PROCESS_ID; //esto es el numero de socket
	t_list * READY_LIST;
	t_list * EXEC_LIST;
	bool bloqueado;
} t_process;

typedef struct t_suse_semaforos{
	char* NAME;
	uint32_t INIT;
	uint32_t MAX;
	uint32_t VALUE;
	t_list * BLOCKED_LIST;
}t_suse_semaforos;


suse_configuration configuracion_suse;
suse_configuration get_configuracion();

pthread_mutex_t mutex_new_queue;
t_list* new_queue;

pthread_mutex_t mutex_ready_queue;
t_list* ready_queue;

pthread_mutex_t mutex_blocked_queue;
t_list* blocked_queue;

pthread_mutex_t mutex_semaforos;

pthread_mutex_t mutex_multiprog;

t_process * generar_programa(int socket_hilolay);

void handle_hilolay(un_socket socket_actual, t_paquete* paquete_hilolay);

void close_tid(int tid);

void handle_close_tid(socket_actual,received_packet);

void handle_wait_sem(socket_actual, received_packet);

int i_thread = 0;
pthread_t threads[20];

pthread_t nuevo_hilo(void *(* funcion ) (void *), t_list * parametros);

t_list* thread_params;

void* programa_conectado_funcion_thread(void* argumentos);



/*
--------------------------------------------------------
----------------- Variables para el Servidor -----------
--------------------------------------------------------
*/
void iniciar_servidor();

void handle_conection_suse(int socketActual);

int listener;     // listening socket descriptors
struct sockaddr_storage remoteaddr; // client address
socklen_t addrlen;
char buf[256];    // buffer for client data
int nbytes;
char remoteIP[INET6_ADDRSTRLEN];
int yes=1;        // for setsockopt() SO_REUSEADDR, below
int i, j, rv;
struct addrinfo hints, *ai, *p;
/*
--------------------------------------------------------
----------------- FIN Variables para el Servidor -------
--------------------------------------------------------
*/


#endif 

