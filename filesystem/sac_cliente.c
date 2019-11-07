#include "sac_cliente.h"

struct t_runtime_options {
	char* welcome_msg;
	char* define_disc_path;
	char* log_level_param;
	char* log_path_param;
} runtime_options;

/*
 * This is the main structure of FUSE with which we tell
 * the library what functions it has to invoke according to the request.
*/
static struct fuse_operations sac_oper = {
		.getattr = sac_cliente_getattr,
		.read = sac_cliente_read,
		.readdir = sac_cliente_readdir,
		.mkdir = sac_cliente_mkdir,
		.rmdir = sac_cliente_rmdir,
		.write = sac_cliente_write,
		.mknod = sac_cliente_mknod,
		.unlink = sac_cliente_unlink,
};

/** keys for FUSE_OPT_ options */

enum {
	KEY_VERSION,
	KEY_HELP,
};

static struct fuse_opt fuse_options[] = {

		// Option that uses the "--Disc-Path" parameter if it is passed:
		CUSTOM_FUSE_OPT_KEY("--Disc-Path=%s", define_disc_path, 0),

		// Define the log level
		CUSTOM_FUSE_OPT_KEY("--ll=%s", log_level_param, 0),

		// Define the log path
		CUSTOM_FUSE_OPT_KEY("--Log-Path", log_path_param, 0),

		// Default fuse parameters
		FUSE_OPT_KEY("-V", KEY_VERSION),
		FUSE_OPT_KEY("--version", KEY_VERSION),
		FUSE_OPT_KEY("-h", KEY_HELP),
		FUSE_OPT_KEY("--help", KEY_HELP),
		FUSE_OPT_END,
};

// This is our path, relative to the mount point, file inside the FS
#define DEFAULT_FILE_PATH "/" DEFAULT_FILE_NAME
#define CUSTOM_FUSE_OPT_KEY(t, p, v) { t, offsetof(struct t_runtime_options, p), v }


int send_call(Function *f)
{
	Message msg;
	MessageHeader header;

	create_message_header(&header,MESSAGE_CALL,1,sizeof(Function));
	create_function_message(&msg,&header,f);

	int resultado = send_message(master_socket,&msg);

	free(msg.data);

	return resultado;
}

int send_path(FuncType func_type, const char *path){
	Function f;
	Arg arg;

	arg.type = VAR_CHAR_PTR;
	arg.size = strlen(path) + 1;
	strcpy(arg.value.val_charptr,path);

	f.type = func_type;
	f.num_args = 1;
	f.args[0] = arg;

	int resultado = send_call(&f);

	return resultado;
}

int sac_cliente_getattr(const char *path, struct stat *stbuf) {
	Message msg;

	memset(stbuf, 0, sizeof(struct stat));

	if(send_path(FUNCTION_GETATTR, path) == -1)
		return EXIT_FAILURE;

	receive_message(master_socket,&msg);

	Function* f = msg.data;

	if(f->type != FUNCTION_RTA_GETATTR){
		return -ENOENT;
	}
	
	stbuf->st_mode = f->args[0].value.val_u32; // fijarse que pegue int 32, sino agregarlo a menssage (junto con type etc) o usar char*
	stbuf->st_nlink = f->args[1].value.val_u32;
	stbuf->st_size = f->args[2].value.val_u32;

	free(f);

	return EXIT_SUCESS;
}


int sac_cliente_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	(void) fi;
	(void) offset;
	Message msg;
	uint8_t tam_dir = 0;
	uint16_t off = 0;

	if(send_path(FUNCTION_READDIR, path) == -1)
			return EXIT_FAILURE;

	receive_message(master_socket,&msg);

	Function* f = msg.data;

	if(f->type != FUNCTION_RTA_READDIR){
		return -ENOENT;
	}

	char* mandar;

	while(off < f->args[0].size){

		memcpy(&tam_dir, f->args[0].value.val_charptr + off, sizeof(uint8_t));
		off = off + sizeof(uint8_t);
		mandar = malloc(tam_dir);
		memcpy(mandar, f->args[0].value.val_charptr + off, tam_dir);
		off = off + tam_dir;
		filler(buf, mandar, NULL, 0);
		free(mandar);

	};
	// no veo que le haga malloc al char*, por las dudas verificar despues
	free(f);

	return EXIT_SUCCESS;
}

int sac_cliente_open(const char *path, struct fuse_file_info *fi) {

	//chequeo si el archivo existe, ignoro permisos

	Message msg;

	if(send_path(FUNCTION_GETATTR, path) == -1)
		return EXIT_FAILURE;

	receive_message(master_socket,&msg);

	int *response = msg.data;
	int respuesta = *response;

	free(msg.data);

	return respuesta; // retorna 0 si existe, o -EACCESS en caso contrario.
}

int sac_cliente_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	Message msg;
	char *lectura;
	
	Function fsend;
	Arg arg[3];

	arg[0].type = VAR_CHAR_PTR;
	arg[0].size = strlen(path) + 1;
	strcpy(arg[0].value.val_charptr,path);

	arg[1].type = VAR_SIZE_T;
	arg[1].size = sizeof(size_t);
	arg[1].value.val_sizet = size;

	arg[2].type = VAR_UINT32; // fijarse si pega con int32, sino agregarlo / usar char*
	arg[2].size = sizeof(uint32_t);
	arg[2].value.val_u32 = offset;

	fsend.type = FUNCTION_READ;
	fsend.num_args = 3;
	fsend.args[0] = arg[0];
	fsend.args[1] = arg[1];
	fsend.args[2] = arg[2];

	if(send_call(&fsend) == -1)
		return EXIT_FAILURE;

	receive_message(master_socket,&msg);

	Function* freceive = msg.data;

	if(freceive->type != FUNCTION_RTA_READ){
		return -ENOENT;
	}

	int tamanioLectura = freceive->args[0].size;

	memcpy(buf, freceive->args[0].value.val_charptr, tamanioLectura);

	// no veo que le haga malloc al char*, por las dudas verificar despues
	free(freceive);
  
  	return tamanioLectura;
}

int sac_cliente_mknod(const char* path, mode_t mode, dev_t rdev){
	Message msg;

	if(send_path(FUNCTION_MKNOD, path) == -1)
		return EXIT_FAILURE;

	receive_message(master_socket,&msg);

	int *response = msg.data;
	int respuesta = *response;

	free(msg.data);

	return respuesta; // retorna 0 si existe, o -EACCESS en caso contrario.
}

int sac_cliente_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){

	Message msg;

	Function f;
	Arg arg[4];

	arg[0].type = VAR_CHAR_PTR;
	arg[0].size = strlen(path) + 1;
	strcpy(arg[0].value.val_charptr,path);

	arg[1].type = VAR_CHAR_PTR;
	arg[1].size = strlen(buf) + 1;
	strcpy(arg[1].value.val_charptr,buf);

	arg[2].type = VAR_SIZE_T;
	arg[2].size = sizeof(size_t);
	arg[2].value.val_sizet = size;

	arg[3].type = VAR_UINT32; // fijarse si pega con int32, sino agregarlo / usar char*
	arg[3].size = sizeof(uint32_t);
	arg[3].value.val_u32 = offset;

	f.type = FUNCTION_WRITE;
	f.num_args = 4;
	f.args[0] = arg[0];
	f.args[1] = arg[1];
	f.args[2] = arg[2];
	f.args[3] = arg[3];

	if(send_call(&f) == -1)
		return EXIT_FAILURE;

	receive_message(master_socket,&msg);

	int *response = msg.data;
	int bytesEscritos = *response;

	free(msg.data);

	return bytesEscritos; // podria verificar que sean los mismos que los pedidos o sino error
}


int sac_cliente_unlink(const char* path){
	Message msg;

	if(send_path(FUNCTION_UNLINK, path) == -1)
		return EXIT_FAILURE;

	receive_message(master_socket,&msg);

	int *response = msg.data;
	int respuesta = *response;

	free(msg.data);

	return respuesta; // retorna 0 si existe, o -EACCESS en caso contrario.
}

int sac_cliente_mkdir(const char* path, mode_t mode){
	Message msg;

	if(send_path(FUNCTION_MKDIR, path) == -1)
		return EXIT_FAILURE;

	receive_message(master_socket,&msg);

	int *response = msg.data;
	int respuesta = *response;

	free(msg.data);

	return respuesta; // retorna 0 si existe, o -EACCESS en caso contrario.
}

int sac_cliente_rmdir(const char* path){
	Message msg;

	if(send_path(FUNCTION_RMDIR, path) == -1)
		return EXIT_FAILURE;

	receive_message(master_socket,&msg);

	int *response = msg.data;
	int respuesta = *response;

	free(msg.data);

	return respuesta; // retorna 0 si existe, o -EACCESS en caso contrario.
}

// Dentro de los argumentos que recibe nuestro programa obligatoriamente
// debe estar el path al directorio donde vamos a montar nuestro FS
int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	////////////////////////////////no se donde hacer el close del socket////////////////////////////////////////
	//esta bastante hardcodeado esto, creo que deberia tomarlo del config
	serverSocket = connect_to("127.0.0.1",8003);

	// Limpio la estructura que va a contener los parametros
	memset(&runtime_options, 0, sizeof(struct t_runtime_options));

	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
		* error parsing options
		perror("Invalid arguments!");
		return EXIT_FAILURE;
	}

	// Si se paso el parametro --welcome-msg
	// el campo welcome_msg deberia tener el
	// valor pasado
	if( runtime_options.welcome_msg != NULL ){
		printf("%s\n", runtime_options.welcome_msg);
	}

	// Esta es la funcion principal de FUSE, es la que se encarga
	// de realizar el montaje, comuniscarse con el kernel, delegar todo
	// en varios threads
	return fuse_main(args.argc, args.argv, &sac_oper, NULL);
}
