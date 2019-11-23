#ifndef hilolay_h__
	#define hilolay_h__

	#define HILOLAY_CONFIG_PATH "../configs/hilolay.config"

	/* Interface for programs, this is what the programs should use and is implemented in hilolay_internal */

	// TODO: Not working in program without struct, even though it has typedef
	typedef struct hilolay_t {
		int tid;
	} hilolay_t;

	typedef struct hilolay_attr_t {
		int attrs;
	} hilolay_attr_t;

	typedef struct hilolay_configuration{
		char* CONNECTION_ADDR;
		int CONNECTION_PORT;
	}hilolay_configuration;

	/**
	 * Initializes the library. Implemented in the client interface.
	 *
	 * Must call hilolay.c#_init function
	 */
	void hilolay_init(void);


	int hilolay_create(hilolay_t *thread, const hilolay_attr_t *attr, void *(*start_routine)(void *), void *arg);


	int hilolay_yield(void);


	int hilolay_join(hilolay_t *thread);


	int hilolay_get_tid(void);
#endif // hilolay_h__
