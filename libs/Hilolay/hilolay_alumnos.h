#ifndef hilolay_alumnos_h__
	#define hilolay_alumnos_h__
	#define HILOLAY_ALUMNO_CONFIG_PATH "../configs/hilolay_alumnos.config"

	typedef struct hilolay_operations {
		int (*suse_create) (int);
		int (*suse_schedule_next) (void);
		int (*suse_join) (int);
		int (*suse_close) (int);
		int (*suse_wait) (int);
		int (*suse_signal) (int);
	} hilolay_operations;

	hilolay_operations *main_ops;

	void init_internal(struct hilolay_operations*);

	typedef struct hilolay_alumno_configuracion {
		char* SUSE_IP;
		char* SUSE_PORT; //Era int, lo paso a char* como lo pide la funcion de conectar_a
	} hilolay_alumnos_configuracion;

	//Inicializa la libreria Hilolay, se conecta con SUSE.
	void hilolay_init(void);

	//Obtiene el numero de socket de conexion con SUSE
	int get_socket_actual();

	int suse_join(int tid);

	//Conexion con suse
	void conectar_con_suse();

	//Retorna la configuracion de hilolay_alumno
	hilolay_alumnos_configuracion get_configuracion_hilolay();

	hilolay_alumnos_configuracion configuracion_hilolay;

	void handle_conection_hilolay(int socket_actual);

	void handle_suse(socket_actual, received_packet);

	int suse_close(int tid);


#endif // hilolay_alumnos_h__
