#ifndef SAC_SERVER_H
#define SAC_SERVER_H

#include "network.h"
#include <commons/log.h>
#include <commons/config.h>
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include <signal.h>
#include <libgen.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>


#define BLOQUE_SIZE 4096
#define MAX_NUMBER_OF_FILES 1024
#define MAGIC_NUMBER_NAME 3
#define HEADER_BLOCK_SIZE 1
#define MAX_NAME_SIZE 71
#define BLKINDIRECT 1000
#define GFILEBYTABLE 1024
#define BITMAP_START_BLOCK 1
#define BITMAP_SIZE_BLOCKS 1
#define SAC_CONFIG_PATH "../configs/filesystem.config"

typedef struct sac_configuration_s{
	int listen_port;
}sac_configuration;

typedef struct fuse_configuration_s
{
	int listen_port;
	uint32_t disk_size;
}fuse_configuration;

typedef uint32_t ptrGBloque;

// sac server block struct
typedef struct sac_server_block{
	unsigned char bytes[BLOQUE_SIZE];
}GBlock;

// sac server header struct
typedef struct sac_server_header{
	unsigned char identificador[MAGIC_NUMBER_NAME];
	uint32_t version;
	uint32_t bitmap_start;
	uint32_t bitmap_size; // in blocks
	unsigned char padding[4081];
}Header;


// sac server file struct
typedef struct sac_server_gfile{
	uint8_t state; // 0 deleted, 1 file, 2 directory
	unsigned char fname[MAX_NAME_SIZE];
	ptrGBloque parent_dir_block;
	uint32_t file_size;
	uint64_t create_date;
	uint64_t modify_date;
	ptrGBloque indirect_blocks_array[BLKINDIRECT];
}GFile;


// memory mapping data definition
struct sac_server_header *header_start;
struct sac_server_gfile *node_table_start, *data_block_start, *bitmap_start;

//semaphore that will be used to write:
pthread_rwlock_t rwlock;
t_log *logger;

// Use this structure to store the descriptor number in which the disk was opened
int discDescriptor;

// Functions to handle the disk

/* * To read, documentation sac_client.h */
int sac_server_getatrr(char* payload);
int sac_server_readdir(char* payload);
int sac_server_read(char* payload);

/* * To write, documentation sac_client.h */
// 00000000000000000000000000 IDK IF THE PARAMETERS ARE GOING TO BE EXACTLY THESE 0000000000000000000000000
int sac_server_mkdir(char* payload);
int sac_server_rmdir(char* payload);
int sac_server_write(char* payload);
int sac_server_mknod(char* payload);
int sac_server_unlink(char* payload);


// Auxiliary structure management functions, located in sac_handlers

/* @DESC
 * 		Determine which is the node where the path is located.
 *
 * 	@PARAM
 * 		path - Path of the directory/file to be found
 *
 * 	@RETURN
 * 		Returns the block number where is the path or -1 if an error ocurred
 *
 */

ptrGBloque determine_node(const char* path);

/*
 * 	@DESC
 * 		Divide the path in the format of [ROUTE] into: [ROUTE_WAY] and [NAME].
 * 		Example:
 * 			path: /home/utnso/algo.txt == /home/utnso - algo.txt
 * 			path: /home/utnso/ == /home - utnso
 *
 * 	@PARAM
 * 		path - Path to be divided
 * 		super_path - Pointer over which the upper route will be saved.
 * 		name - Pointer to the file name
 *
 * 	@RET
 * 		0 in all situations
 *
 */

int split_path(const char* path, char** super_path, char** name);


/*
 * 	@DESC
 *		Evaluate if the last character is chr
 *
 * 	@PARAM
 * 		str - String to be evaluated
 * 		chr - Expected last char
 *
 * 	@RET
 * 		1 if last char is the expected, 0 in other case
 *
 */
int lastchar(const char* str, char chr);

/*
 Starts server,creates a logger and loads configuration
int sac_start_service(ConnectionHandler ch);

 Stops server,frees resources
void sac_stop_service();

 Decides wich message has arrived
 * and depending on the message 
 * does an action
void message_handler(Message *m,int sock);
*/

#endif
