#include "sac_cliente.h"

int send_call(Function *f)
{
	Message* msg = malloc(sizeof(Message));
	MessageHeader header;

	create_message_header(&header,MESSAGE_CALL,1,sizeof(Function));
	create_function_message(msg,&header,f);

	int resultado = send_message(serverSocket,msg);

	free(msg);
	msg = NULL;

	return resultado;

}

int send_path(FuncType func_type, const char *path){
	Function f;

	f.args[0].type = VAR_CHAR_PTR;
	f.args[0].size = strlen(path) + 1;
	f.args[0].value.val_charptr = malloc(f.args[0].size); //Aca hay memory leaks segun Valgrind
	memcpy(f.args[0].value.val_charptr, path, f.args[0].size);

	f.type = func_type;
	f.num_args = 1;

	int resultado = send_call(&f);

	free(f.args[0].value.val_charptr);
	f.args[0].value.val_charptr = NULL;

	return resultado;
}

static int sac_getattr(const char *path, struct stat *stbuf) {
	Message msg;

	memset(stbuf, 0, sizeof(struct stat));

	if(send_path(FUNCTION_GETATTR, path) == -1){
		return EXIT_FAILURE;
	}

	receive_message(serverSocket,&msg);

	Function* f = msg.data;

	if(f->type == FUNCTION_RTA_GETATTR_NODORAIZ){
		stbuf->st_mode = f->args[0].value.val_u32;
		stbuf->st_nlink = f->args[1].value.val_u32;

		free(msg.data);
		msg.data = NULL;

		return EXIT_SUCCESS;
	}

	if(f->type != FUNCTION_RTA_GETATTR){
		free(msg.data);
		msg.data = NULL;
		return -ENOENT;
	}
	
	stbuf->st_mode = f->args[0].value.val_u32; // fijarse que pegue int 32, sino agregarlo a menssage (junto con type etc) o usar char*
	stbuf->st_nlink = f->args[1].value.val_u32;
	stbuf->st_size = f->args[2].value.val_u32;

	free(msg.data);
	msg.data = NULL;

	return EXIT_SUCCESS;
}


static int sac_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	(void) fi;
	(void) offset;
	Message msg;
	uint8_t tam_dir = 0;
	uint16_t off = 0;

	if(send_path(FUNCTION_READDIR, path) == -1){
			return EXIT_FAILURE;
	}

	receive_message(serverSocket,&msg);

	Function* f = msg.data;

	if(f->type != FUNCTION_RTA_READDIR){
		free(msg.data);
		msg.data = NULL;
		return -ENOENT;
	}

	int dimListaSpliteada;
	char **listaSpliteada;
	char* filename;

	//filler(buf, ".", NULL, 0);
	//filler(buf, "..", NULL, 0);

	if (f->args[0].value.val_charptr[0] == '\0')
	{
		free(f->args[0].value.val_charptr);
		f->args[0].value.val_charptr = NULL;
		free(msg.data);
		return 0;
	}

	listaSpliteada = string_split(f->args[0].value.val_charptr, "/");
	dimListaSpliteada = largoListaString(listaSpliteada);

	for(int i=0 ; i<dimListaSpliteada; i++){

		filename = listaSpliteada[i];
		filler(buf, filename, NULL, 0);
		free(listaSpliteada[i]);
		listaSpliteada[i] = NULL;
	}

	free(listaSpliteada);
	listaSpliteada = NULL;
	free(f->args[0].value.val_charptr);
	f->args[0].value.val_charptr = NULL;
	free(msg.data);
	msg.data = NULL;

	return EXIT_SUCCESS;
}

static int sac_open(const char *path, struct fuse_file_info *fi) {

	//chequeo si el archivo existe, ignoro permisos

	Message msg;

	if(send_path(FUNCTION_OPEN, path) == -1){
		return EXIT_FAILURE;
	}

	receive_message(serverSocket,&msg);

	Function* f = msg.data;

	if(f->type != FUNCTION_RTA_OPEN){
		free(msg.data);
		msg.data = NULL;
		return -EACCES;
	}

	int respuesta = f->args[0].value.val_u32;

	free(msg.data);
	msg.data = NULL;

	return respuesta; // retorna 0 si existe, o -EACCESS en caso contrario.
}

static int sac_opendir(const char *path, struct fuse_file_info *fi){
	Message msg;

	if(send_path(FUNCTION_OPENDIR, path) == -1){
		return EXIT_FAILURE;
	}

	receive_message(serverSocket,&msg);

	Function* f = msg.data;

	if(f->type != FUNCTION_RTA_OPENDIR){
		free(msg.data);
		msg.data = NULL;
		return -EACCES;
	}

	int respuesta = f->args[0].value.val_u32;

	free(msg.data);
	msg.data = NULL;

	return respuesta; // retorna 0 si existe, o -EACCESS en caso contrario.
}

static int sac_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	Message msg;
	char *lectura;
	
	Function fsend;

	fsend.args[0].type = VAR_SIZE_T;
	fsend.args[0].size = sizeof(size_t);
	fsend.args[0].value.val_sizet = size;

	fsend.args[1].type = VAR_UINT32; // fijarse si pega con int32, sino agregarlo / usar char*
	fsend.args[1].size = sizeof(uint32_t);
	fsend.args[1].value.val_u32 = offset;

	fsend.args[2].type = VAR_CHAR_PTR;
	fsend.args[2].size = strlen(path) + 1;
	fsend.args[2].value.val_charptr = malloc(fsend.args[2].size);
	memcpy(fsend.args[2].value.val_charptr, path, fsend.args[2].size);

	fsend.type = FUNCTION_READ;
	fsend.num_args = 3;

	if(send_call(&fsend) == -1){
		return EXIT_FAILURE;
	}

	//free(fsend.args[0].value.val_charptr);

	receive_message(serverSocket,&msg);

	Function* freceive = msg.data;

	if(freceive->type != FUNCTION_RTA_READ){
		free(msg.data);
		msg.data = NULL;
		return -ENOENT;
	}

	int tamanioLectura = freceive->args[0].size;

	if(tamanioLectura == 0){
		free(freceive->args[0].value.val_charptr);
		freceive->args[0].value.val_charptr = NULL;
		free(freceive);
		freceive = NULL;

		return tamanioLectura;
	}

	memcpy(buf, freceive->args[0].value.val_charptr, tamanioLectura);

	free(freceive->args[0].value.val_charptr);
	freceive->args[0].value.val_charptr = NULL;
	free(freceive);
	freceive = NULL;
  
  	return tamanioLectura;
}

static int sac_mknod(const char* path, mode_t mode, dev_t rdev){
	Message msg;

	if(send_path(FUNCTION_MKNOD, path) == -1){
		return EXIT_FAILURE;
	}

	receive_message(serverSocket,&msg);

	Function* f = msg.data;

	if(f->type != FUNCTION_RTA_MKNOD){
		free(msg.data);
		msg.data = NULL;
		return -1;
	}

	int respuesta = f->args[0].value.val_u32;

	free(msg.data);
	msg.data = NULL;

	return respuesta;
}

static int sac_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){

	Message msg;

	Function f;

	f.type = FUNCTION_WRITE;
	f.num_args = 3;

	f.args[1].type = VAR_CHAR_PTR;
	f.args[1].size = strlen(path) + 1;
	f.args[1].value.val_charptr = malloc(f.args[1].size);
	memcpy(f.args[1].value.val_charptr, path, f.args[1].size);

	f.args[2].type = VAR_CHAR_PTR;
	f.args[2].size = size;
	f.args[2].value.val_charptr = malloc(f.args[2].size);
	memcpy(f.args[2].value.val_charptr, buf, f.args[2].size);

	//f.args[1].type = VAR_SIZE_T;
	//f.args[1].size = sizeof(size_t);
	//f.args[1].value.val_sizet = size;

	f.args[0].type = VAR_UINT32; // fijarse si pega con int32, sino agregarlo / usar char*
	f.args[0].size = sizeof(uint32_t);
	f.args[0].value.val_u32 = offset;

	if(send_call(&f) == -1){
		return EXIT_FAILURE;
	}

	//free(f.args[1].value.val_charptr);
	//free(f.args[0].value.val_charptr);

	receive_message(serverSocket,&msg);

	Function* fresp = msg.data;

	if(fresp->type != FUNCTION_RTA_WRITE){
		free(msg.data);
		msg.data = NULL;
		return -ENOENT;
	}

	int bytesEscritos = fresp->args[0].value.val_u32;

	free(msg.data);
	msg.data = NULL;

	return bytesEscritos; // podria verificar que sean los mismos que los pedidos o sino error
}


static int sac_unlink(const char* path){
	Message msg;

	if(send_path(FUNCTION_UNLINK, path) == -1){
		return EXIT_FAILURE;
	}

	receive_message(serverSocket,&msg);

	Function* fresp = msg.data;

	if(fresp->type != FUNCTION_RTA_UNLINK){
		free(msg.data);
		msg.data = NULL;
		return -ENOENT;
	}

	int respuesta = fresp->args[0].value.val_u32;

	//Aca deberia agarrar el valor de bytes borrados, ponerlo en un log y mostrarlo. Finalmente retornar 0 si borro >=0 bytes, si es -1 error

	free(msg.data);
	msg.data = NULL;

	return respuesta; //Devuelve 0 si todo bien, -1 si todo mal
}

static int sac_mkdir(const char* path, mode_t mode){
	Message msg;

	if(send_path(FUNCTION_MKDIR, path) == -1){
		return EXIT_FAILURE;
	}

	receive_message(serverSocket,&msg);

	Function* fresp = msg.data;

	if(fresp->type != FUNCTION_RTA_MKDIR){
		free(msg.data);
		msg.data = NULL;
		return -1;
	}

	int respuesta = fresp->args[0].value.val_u32;

	free(msg.data);
	msg.data = NULL;

	return respuesta;
}

static int sac_rmdir(const char* path){
	Message msg;

	if(send_path(FUNCTION_RMDIR, path) == -1){
		return EXIT_FAILURE;
	}

	receive_message(serverSocket,&msg);

	Function* fresp = msg.data;

	if(fresp->type != FUNCTION_RTA_RMDIR){
		free(msg.data);
		msg.data = NULL;
		return -ENOENT;
	}

	int respuesta = fresp->args[0].value.val_u32;

	free(msg.data);
	msg.data = NULL;

	return respuesta; // retorna 0 si existe, o -EACCESS en caso contrario.
}

static struct fuse_operations sac_oper = {
		.getattr = sac_getattr,
		.read = sac_read,
		.readdir = sac_readdir,
		.open = sac_open,
		.opendir = sac_opendir,
		.mkdir = sac_mkdir,
		.rmdir = sac_rmdir,
		.write = sac_write,
		.mknod = sac_mknod,
		.unlink = sac_unlink,
};

// Dentro de los argumentos que recibe nuestro programa obligatoriamente
// debe estar el path al directorio donde vamos a montar nuestro FS
int main(int argc, char *argv[]){
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	////////////////////////////////no se donde hacer el close del socket////////////////////////////////////////
	//esta bastante hardcodeado esto, creo que deberia tomarlo del config
	serverSocket = connect_to("127.0.0.1",8003);

	// Esta es la funcion principal de FUSE, es la que se encarga
	// de realizar el montaje, comuniscarse con el kernel, delegar todo
	// en varios threads
	return fuse_main(args.argc, args.argv, &sac_oper, NULL);
}
