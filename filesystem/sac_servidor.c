#include "sac_servidor.h"
#include <sys/stat.h>
#include <fcntl.h>
#include "sac_handlers.h"

Function fuse_invoke_function(Function *f);


void dic_data_destroyer(void* data){
	free(data);
	data = NULL;
}

void fuse_stop_service()
{
	log_info(fuse_logger,"SIGINT recibida. Servidor desconectado!");
	free(fuse_config->path_archivo);
	free(fuse_config);
	log_destroy(fuse_logger);
	bitarray_destroy(bitmap);
	dictionary_destroy_and_destroy_elements(diccionarioDeMutex, &dic_data_destroyer);
	server_stop();
}

fuse_configuration* load_configuration(char *path)
{
	t_config *config = config_create(path);
	fuse_configuration *fc = (fuse_configuration *)malloc(sizeof(fuse_configuration));
	if(config == NULL)
	{
		log_error(fuse_logger,"No se pudo cargar la configuracion. Apagando servidor...!");
		fuse_stop_service();
		exit(-1);
	}

	fc->path_archivo = malloc(strlen(config_get_string_value(config,"PATH_ARCHIVO")) + 1); //aca hay memory leaks

	fc->listen_port = config_get_int_value(config,"LISTEN_PORT");
	fc->disk_size = config_get_int_value(config,"DISK_SIZE");
	strcpy(fc->path_archivo,config_get_string_value(config,"PATH_ARCHIVO"));

	config_destroy(config);
	return fc;
}

void inicioTablaDeNodos(){
	int tamanioHeader = 1;
	double diskSize = (double) fuse_config->disk_size;
	double tamanioBloque = (double) BLOQUE_SIZE;
	double divisionUno = diskSize / tamanioBloque;
	double divisionDos = divisionUno / (double) 8;
	double divisionTres = divisionDos / tamanioBloque;
	int tamanioBitmapBloque = ceil(divisionTres);
	cantidadDeBloquesBitmap = tamanioBitmapBloque;

	bloqueInicioTablaDeNodos = tamanioHeader + tamanioBitmapBloque;
}

void inicioBloquesDeDatos(){
	bloqueInicioBloquesDeDatos = bloqueInicioTablaDeNodos + MAX_NUMBER_OF_FILES + 1;
}

void configurar_server(){
	int fileDescriptor = 0;
	diskSize = fuse_config->disk_size;
	char *pathArchivo = fuse_config->path_archivo;
	fileDescriptor = open(pathArchivo, O_RDWR, 0);

	disco = (GBlock*) mmap(NULL, diskSize, PROT_READ|PROT_WRITE, MAP_SHARED,fileDescriptor,0);

	inicioTablaDeNodos();
	inicioBloquesDeDatos();

	bitmap = bitarray_create_with_mode((char *)(disco + 1), BLOQUE_SIZE * cantidadDeBloquesBitmap, MSB_FIRST);
	diccionarioDeMutex = dictionary_create();

	tablaDeNodos = (GFile*) (disco + bloqueInicioTablaDeNodos);
	bloquesDeDatos = disco + bloqueInicioBloquesDeDatos;

	printf("\033[0;34m");
	printf("-----------Contenido de la tabla de nodos-----------\n");
	printf("\033[0m");

	//es para monitorear los archivos que hay en el filesystem, solo para testing
	for(int i=0; i<100; i++){

		printf("Nodo:%d Bloque:%d Estado:%d Size: %d Nombre:%s Bloque padre:%d\n",
						i,
						bloqueInicioTablaDeNodos + i,
						tablaDeNodos[i].state,
						tablaDeNodos[i].file_size,
						tablaDeNodos[i].fname,
						tablaDeNodos[i].parent_dir_block);
		printf("------------------------------------------------------------\n");

	}

	printf("\033[0;34m");
	printf("-----------Fin tabla de nodos-----------\n");
	printf("\033[0m");
}

void fuse_start_service(ConnectionHandler ch)
{
	fuse_config = load_configuration(SAC_CONFIG_PATH);
	fuse_logger = log_create("/home/utnso/tp-2019-2c-Los-Trapitos/logs/fuse.log","FUSE",true,LOG_LEVEL_TRACE);
	configurar_server();
	//fuse_logger = log_create("../logs/fuse.log","FUSE",true,LOG_LEVEL_TRACE);
	server_start(fuse_config->listen_port,ch);
}

void message_handler(Message *m,int sock)
{
	Function frespuesta;
	Message msg;
	MessageHeader head;
	switch(m->header.message_type)
	{
		case MESSAGE_CALL:
			log_trace(fuse_logger,"Enviando respuesta...");
			frespuesta = fuse_invoke_function((Function *)m->data);
			create_message_header(&head,MESSAGE_CALL,1,sizeof(char)  * tamDataFunction(frespuesta));
			create_function_message(&msg,&head,&frespuesta);
			send_message(sock,&msg);
			log_trace(fuse_logger,"Respuesta enviada");

			liberarMemoria(&frespuesta);

			liberarMemoria((Function *)m->data);
			free(m->data);
			m->data = NULL;

			break;
		default:
			log_error(fuse_logger,"Undefined message");
			break;
	}
}

void* handler(void *args)
{
	ConnectionArgs *conna = (ConnectionArgs *)args;
	Message msg;
	void* buffer;
	int n=0;
	int sock = conna->client_fd;
	struct sockaddr_in client_address = conna->client_addr;

	printf("Un cliente se ha conectado!\n");

	while((n=receive_packet_var(sock,&buffer)) > 0)
	{
		if((n = message_decode(buffer,n,&msg)) > 0)
		{
			message_handler(&msg,sock);
			free(buffer);
		}
		else{
			liberarMemoria((Function *)&msg);
			free(msg.data);
			msg.data = NULL;
			free(buffer);
		}
	}	
	log_debug(fuse_logger,"El cliente se desconecto!");
	free(buffer);
	close(sock);
	return (void*)NULL;
}

Function fuse_invoke_function(Function *f)
{
	Function func_ret;
	switch(f->type)
	{
		case FUNCTION_GETATTR:
			log_info(fuse_logger,"Getattr llamado with -> Path: %s", f->args[0].value.val_charptr);
			func_ret = sac_server_getattr(f->args[0].value.val_charptr);
			break;
		case FUNCTION_READDIR:
			log_info(fuse_logger,"Readdir llamado with -> Path: %s", f->args[0].value.val_charptr);
			func_ret = sac_server_readdir(f->args[0].value.val_charptr);
			break;
		case FUNCTION_OPEN:
			log_info(fuse_logger,"Open llamado with -> Path: %s", f->args[0].value.val_charptr);
			func_ret = sac_server_open(f->args[0].value.val_charptr);
			break;
		case FUNCTION_READ:
			log_info(fuse_logger,"Read llamado with -> Path: %s  Size: %d Offset: %d", f->args[1].value.val_charptr, f->args[2].value.val_u32, f->args[0].value.val_u32);
			func_ret = sac_server_read(f->args[1].value.val_charptr, f->args[2].value.val_u32, f->args[0].value.val_u32);
			break;
		case FUNCTION_OPENDIR:
			log_info(fuse_logger,"Opendir llamado with -> Path: %s", f->args[0].value.val_charptr);
			func_ret = sac_server_opendir(f->args[0].value.val_charptr);
			break;
		case FUNCTION_MKNOD:
			log_info(fuse_logger,"Mknod llamado with -> Path: %s", f->args[0].value.val_charptr);
			func_ret = sac_server_mknod(f->args[0].value.val_charptr);
			break;
		case FUNCTION_WRITE:
			log_info(fuse_logger,"Write llamado with -> Path: %s Pide Escribir: %s Size: %d Offset: %d", f->args[1].value.val_charptr, f->args[2].value.val_charptr, f->args[2].size, f->args[0].value.val_u32);
			func_ret = sac_server_write(f->args[1].value.val_charptr, f->args[2].value.val_charptr, f->args[2].size, f->args[0].value.val_u32);
			break;
		case FUNCTION_UNLINK:
			log_info(fuse_logger,"Unlink llamado with -> Path: %s", f->args[0].value.val_charptr);
			func_ret = sac_server_unlink(f->args[0].value.val_charptr);
			break;
		case FUNCTION_MKDIR:
			log_info(fuse_logger,"Mkdir llamado with -> Path: %s", f->args[0].value.val_charptr);
			func_ret = sac_server_mkdir(f->args[0].value.val_charptr);
			break;
		case FUNCTION_RMDIR:
			log_info(fuse_logger,"Rmdir llamado with -> Path: %s", f->args[0].value.val_charptr);
			func_ret = sac_server_rmdir(f->args[0].value.val_charptr);
			break;
		case FUNCTION_TRUNCATE:
			log_info(fuse_logger,"Truncate llamado with -> Path: %s, Size: %d", f->args[1].value.val_charptr, f->args[0].value.val_u32);
			func_ret = sac_server_truncate(f->args[1].value.val_charptr, f->args[0].value.val_u32);
			break;
		case FUNCTION_RENAME:
			log_info(fuse_logger,"Rename llamado with -> Path viejo: %s, Path nuevo: %s", f->args[0].value.val_charptr, f->args[1].value.val_charptr);
			func_ret = sac_server_rename(f->args[0].value.val_charptr, f->args[1].value.val_charptr);
			break;
		default:
			log_error(fuse_logger,"Funcion desconocida");
			break;
	}
	return func_ret;

}

Function sac_server_rename(char* path, char* nuevoPath){
	int nodoBuscadoPosicion = determine_nodo(path);
	int respuesta = renombrar_archivo(nodoBuscadoPosicion, path, nuevoPath);
	return enviar_respuesta_basica(respuesta, FUNCTION_RTA_RENAME);
}


Function sac_server_truncate(char* path, uint32_t size){
	int nodoBuscadoPosicion = determine_nodo(path);
	int respuesta = truncar_archivo(nodoBuscadoPosicion, size);
	return enviar_respuesta_basica(respuesta, FUNCTION_RTA_TRUNCATE);
}

Function sac_server_open(char* path){
	return validarSiExiste(path, FUNCTION_RTA_OPEN);
}

Function sac_server_opendir(char* path){
	return validarSiExiste(path, FUNCTION_RTA_OPENDIR);
}

Function sac_server_getattr(char* path){
	Message msg;
	Function fsend;
	Arg arg[4];

	uint32_t modo;
	uint32_t nlink_t;
	uint32_t total_size;

	if(!strcmp(path, "/")){
		Arg argNodoRaiz[2];
		argNodoRaiz[0].type = VAR_UINT32;
		argNodoRaiz[0].size = sizeof(uint32_t);
		argNodoRaiz[0].value.val_u32 = (S_IFDIR | 0777);

		argNodoRaiz[1].type = VAR_UINT32;
		argNodoRaiz[1].size = sizeof(uint32_t);
		argNodoRaiz[1].value.val_u32 = 1;

		fsend.type = FUNCTION_RTA_GETATTR_NODORAIZ;
		fsend.num_args = 2;
		fsend.args[0] = argNodoRaiz[0];
		fsend.args[1] = argNodoRaiz[1];
		return fsend;
	}

	int nodoBuscadoPosicion = determine_nodo(path);

	if(nodoBuscadoPosicion == -1){
		log_error(fuse_logger, "El Path ingresado no corresponde con un nodo existente.");
		return retornar_error(fsend);
	}

	bool esArchivo = tablaDeNodos[nodoBuscadoPosicion].state == 1;

	modo = esArchivo == 1 ? (S_IFREG | 0777) : (S_IFDIR | 0777);
	nlink_t = 1;
	total_size = tablaDeNodos[nodoBuscadoPosicion].file_size;
	uint64_t fechaModificacion = tablaDeNodos[nodoBuscadoPosicion].modify_date;

	log_info(fuse_logger, "Nodo encontrado -> Numero nodo: %d, Estado: %d, Size: %d, Bloque padre: %d",
				nodoBuscadoPosicion,
				tablaDeNodos[nodoBuscadoPosicion].state,
				tablaDeNodos[nodoBuscadoPosicion].file_size,
				tablaDeNodos[nodoBuscadoPosicion].parent_dir_block);

	arg[0].type = VAR_UINT32;
	arg[0].size = sizeof(uint32_t);
	arg[0].value.val_u32 = modo;

	arg[1].type = VAR_UINT32;
	arg[1].size = sizeof(uint32_t);
	arg[1].value.val_u32 = nlink_t;

	arg[2].type = VAR_UINT32;
	arg[2].size = sizeof(uint32_t);
	arg[2].value.val_u32 = total_size;

	arg[3].type = VAR_UINT32;
	arg[3].size = sizeof(uint64_t);
	arg[3].value.val_u32 = fechaModificacion;

	fsend.type = FUNCTION_RTA_GETATTR;
	fsend.num_args = 4;
	fsend.args[0] = arg[0];
	fsend.args[1] = arg[1];
	fsend.args[2] = arg[2];
	fsend.args[3] = arg[3];

	return fsend;
}

Function sac_server_read(char* path, size_t size, uint32_t offset){
	Message msg;
	Function fsend;
	int sizeRespuesta;

	char buffer[size];
	memset(buffer, 0, size);

	sizeRespuesta = leer_archivo(buffer, path, size, offset);

	log_info(fuse_logger, "Fin lectura -> Bytes leidos: %d", sizeRespuesta);

	fsend.type = FUNCTION_RTA_READ;
	fsend.num_args = 1;
	fsend.args[0].type = VAR_CHAR_PTR;
	fsend.args[0].size = sizeRespuesta;
	fsend.args[0].value.val_charptr = malloc(fsend.args[0].size);
	memcpy(fsend.args[0].value.val_charptr, buffer, fsend.args[0].size);

	return fsend;
}

Function sac_server_write(char* path, char* buf, size_t size, uint32_t offset){
	Message msg;
	Function fsend;
	int respuesta;

	respuesta = escribir_archivo(buf, path, size, offset);

	log_info(fuse_logger, "Fin escritura -> Bytes escritos: %d", respuesta);

	fsend.type = FUNCTION_RTA_WRITE;
	fsend.num_args = 1;
	fsend.args[0].type = VAR_UINT32;
	fsend.args[0].size = sizeof(uint32_t);
	fsend.args[0].value.val_u32 = respuesta;

	return fsend;
}

Function sac_server_readdir (char* path) {
	Message msg;
	Function fsend;
	char* listaNombres;

	t_list* listaDeArchivos = list_create();

	if(!strcmp(path, "/")){
		completar_lista_con_fileNames(listaDeArchivos, 0);
	}else{
		int nodoPadre = determine_nodo(path);
		completar_lista_con_fileNames(listaDeArchivos, nodoPadre + bloqueInicioTablaDeNodos);
	}

	if(list_size(listaDeArchivos) == 0){
		char* stringVacia[1];
		stringVacia[0] = '\0';

		fsend.type = FUNCTION_RTA_READDIR;
		fsend.num_args = 1;
		fsend.args[0].type = VAR_CHAR_PTR;
		fsend.args[0].size = 1;
		fsend.args[0].value.val_charptr = malloc(fsend.args[0].size); //Aca hay memory leaks
		memcpy(fsend.args[0].value.val_charptr, stringVacia, fsend.args[0].size);

		list_destroy(listaDeArchivos);

		return fsend;
	}

	lista_a_string(listaDeArchivos, &listaNombres);

	fsend.type = FUNCTION_RTA_READDIR;
	fsend.num_args = 1;
	fsend.args[0].type = VAR_CHAR_PTR;
	fsend.args[0].size = strlen(listaNombres) + 1;
	fsend.args[0].value.val_charptr = malloc(fsend.args[0].size);
	memcpy(fsend.args[0].value.val_charptr, listaNombres, fsend.args[0].size);

	list_destroy(listaDeArchivos);
	free(listaNombres);

	return fsend;
}

Function sac_server_mknod (char* path){
	int respuesta = crear_nuevo_nodo(path, 1);
	return enviar_respuesta_basica(respuesta, FUNCTION_RTA_MKNOD);
}

Function sac_server_mkdir (char* path){
	int respuesta = crear_nuevo_nodo(path, 2);
	return enviar_respuesta_basica(respuesta, FUNCTION_RTA_MKDIR);

}

Function sac_server_unlink (char* path){
	int nodoABorrarPosicion = determine_nodo(path);
	GFile *nodoABorrar = tablaDeNodos + nodoABorrarPosicion;

	int respuesta = borrar_archivo(nodoABorrar, nodoABorrarPosicion);
	return enviar_respuesta_basica(respuesta, FUNCTION_RTA_UNLINK);
}

Function sac_server_rmdir (char* path){
	int nodoABorrarPosicion = determine_nodo(path);
	GFile *nodoABorrar = tablaDeNodos + nodoABorrarPosicion;

	int respuesta = borrar_directorio(nodoABorrar, nodoABorrarPosicion);
	return enviar_respuesta_basica(respuesta, FUNCTION_RTA_RMDIR);
}

void liberarMemoria(Function* f){
	switch(f->type){
		case FUNCTION_GETATTR:
		case FUNCTION_READDIR:
		case FUNCTION_OPEN:
		case FUNCTION_OPENDIR:
		case FUNCTION_MKNOD:
		case FUNCTION_UNLINK:
		case FUNCTION_MKDIR:
		case FUNCTION_RMDIR:
		case FUNCTION_RTA_READ:
		case FUNCTION_RTA_READDIR:
			free(f->args[0].value.val_charptr);
			f->args[0].value.val_charptr = NULL;
			break;
		case FUNCTION_READ:
		case FUNCTION_TRUNCATE:
			free(f->args[1].value.val_charptr);
			f->args[1].value.val_charptr = NULL;
			break;
		case FUNCTION_WRITE:
			free(f->args[1].value.val_charptr);
			f->args[1].value.val_charptr = NULL;
			free(f->args[2].value.val_charptr);
			f->args[2].value.val_charptr = NULL;
			break;
		case FUNCTION_RENAME:
			free(f->args[0].value.val_charptr);
			f->args[0].value.val_charptr = NULL;
			free(f->args[1].value.val_charptr);
			f->args[1].value.val_charptr = NULL;
			break;
		default:
			break;
	}
}

int main(int argc,char *argv[])
{
	signal(SIGINT,fuse_stop_service);

	fuse_start_service(handler);

	return 0;
}
