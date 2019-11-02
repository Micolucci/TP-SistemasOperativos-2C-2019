#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "fuse.h"
#include <commons/list.h>

#define MAX_BUFFER 1024

typedef struct {
	int8_t type;
	int16_t length;
} __attribute__ ((__packed__)) tHeader;

typedef struct {
	int8_t type;
	int16_t length;
	char* payload;
} tPaquete;

typedef enum {
	/* Filesystem functions  */
	FF_GETATTR,
	RTA_GETATTR,
	FF_READDIR,
	RTA_READDIR,
	FF_OPEN,
	RTA_OPEN,
	FF_READ,
	RTA_READ,
	FF_MKDIR,
	RTA_MKDIR,
	FF_RMDIR,
	FF_WRITE,
	RTA_WRITE,
	FF_MKNOD,
	RTA_MKNOD,
	FF_UNLINK,
	RTA_UNLINK,
	FF_ERROR,
	DESCONEXION
} tMessage;

typedef struct {
	mode_t modo;
	nlink_t nlink;
	off_t total_size;
}t_GetAttrResp;

typedef struct {
	uint16_t tamano;
	char* lista_nombres;
}t_ReaddirResp;

// Definition of payloads to be send in the package

// ------------ el contenido de las struct esta mal, deberian ser los parametros de las funciones
// que va a utilizar el sac_server ej de memcpy-----------------------------------------

typedef struct {
	uint16_t length;
	char *path;
} t_Path;

typedef struct{
	const char *path;
	void *buf;
	fuse_fill_dir_t filler;
	off_t offset;
	struct fuse_file_info *fi;
} t_Readdir;

typedef struct{
	uint16_t pathLength;
	const char *path;
	size_t size;
	off_t offset;
} t_Read;

typedef struct{
	uint16_t pathLength;
	const char *path;
	uint16_t bufLength;
	const char *buf;
	size_t size;
	off_t offset;
} t_Write;

typedef struct{
	const char *pathname;
	mode_t mode;
} t_Mkdir;

typedef struct{
	const char *pathname;
	mode_t mode;
	dev_t dev;
} t_Mknod;

typedef struct{
	const char *pathname;
} t_Rmdir;

typedef struct{
	const char *pathname;
} t_Unlink;


// encode / decode

int int_encode(tMessage message_type, int num, tPaquete* pPaquete);
int int_decode(char* payload);

int path_encode(tMessage message_type, const char* path, tPaquete* pPaquete);
int path_decode(char* payload, t_Path* tipoPath);

int readdir_encode(tMessage message_type, t_Readdir parameters, tPaquete* pPaquete);
int readdir_decode(char* payload, t_Readdir* tipoReaddir);

int read_encode(tMessage message_type, t_Read parameters, tPaquete* pPaquete);
int read_decode(char* payload, t_Read* tipoRead);

int mkdir_encode(tMessage message_type, t_Mkdir parameters, tPaquete* pPaquete);
int mkdir_decode(char* payload, t_Mkdir* tipoMkdir);

int mknod_encode(tMessage message_type, t_Mknod parameters, tPaquete* pPaquete);
int mknod_decode(char* payload, t_Mknod* tipoMknod);

int rmdir_encode(tMessage message_type, t_Rmdir parameters, tPaquete* pPaquete);
int rmdir_decode(char* payload, t_Rmdir* tipoRmdir);

int unlink_encode(tMessage message_type, t_Unlink parameters, tPaquete* pPaquete);
int unlink_decode(char* payload, t_Unlink* tipoUnlink);

int write_encode(tMessage message_type, t_Write parameters, tPaquete* pPaquete);
int write_decode(char* payload, t_Write* tipoWrite);

int serializar_Gettattr_Resp(t_GetAttrResp parametros, tPaquete *paquete);
int deserializar_Gettattr_Resp(char *payload, t_GetAttrResp* tipoAtrrResp);

int deserializar_Readdir_Rta(uint16_t payloadLength, void *payload, t_ReaddirResp* tipoReaddirResp);


