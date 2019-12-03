/*
* main_memory.c
* Estructura administrativa de la memoria,
* Permite inizializar memoria, liberarla, 
* buscar un programa en la tabla de programas, 
* y buscar páginas libre
*/

#include "main_memory.h"
#include <fcntl.h> //for open() funct
#include <sys/mman.h> //for mmap() & munmap()
#include <sys/stat.h>

#define METADATA_SIZE sizeof(struct HeapMetadata)

void *MAIN_MEMORY = NULL;

/*int floor(float num)
{
    return (int)num;
}*/

t_list *program_list = NULL;
t_list *segment_list = NULL;
t_log *metricas_logger = NULL;
t_log *debug_logger = NULL;

int PAGE_SIZE = 0;
int TOTAL_FRAME_NUM = 0;
bool *BITMAP;

void muse_main_memory_init(int memory_size, int page_size)
{
	int i;
	heap_metadata* metadata;
	int curr_page_num;	
	void *mem_ptr = MAIN_MEMORY;
	PAGE_SIZE = page_size;

	MAIN_MEMORY = (void *) malloc(memory_size); // aka UPCM

	metadata = (heap_metadata*) malloc(METADATA_SIZE);
	TOTAL_FRAME_NUM = (memory_size / PAGE_SIZE);
	*BITMAP = BITMAP[TOTAL_FRAME_NUM]; //1 if free;

	program_list = list_create();
	segment_list = list_create();

	debug_logger = log_create(MUSE_LOG_PATH,"DEBUG", true,LOG_LEVEL_TRACE);
	metricas_logger = log_create(MUSE_LOG_PATH, "METRICAS", true, LOG_LEVEL_TRACE);

	log_trace(metricas_logger, "Tamaño de pagina = tamaño de frame: %d", PAGE_SIZE);
	log_trace(metricas_logger, "Tamaño de metadata: %d", METADATA_SIZE);	
	log_trace(metricas_logger, "Cantidad Total de Memoria:%d", memory_size);
	number_of_free_frames();	
}

void muse_main_memory_stop()
{
	free(MAIN_MEMORY);

	list_destroy(program_list);
	list_destroy(segment_list);

	log_destroy(metricas_logger);
	log_destroy(debug_logger);
}

int search_program(uint32_t pid)
{
	int i=0;
	program *prog;
	while(i<list_size(program_list))
	{
		prog = list_get(program_list, i);
		if(prog->pid == pid) {return i;}
		i++;
	}
	return -1;
}

int number_of_free_frames(){
	int memoria_libre = 0; 
	int frames_libres = 0;
	int i;

	for(i=0; i < TOTAL_FRAME_NUM; i++)
	{	
		frames_libres += (BITMAP[i]);
	}

	memoria_libre = frames_libres * PAGE_SIZE;

	log_trace(metricas_logger, "Cantidad de Memoria libre:%d ", memoria_libre);	
	log_trace(metricas_logger, "Cantidad de Frames libres: %d / %d ", frames_libres, TOTAL_FRAME_NUM);

	return frames_libres;
}

int proximo_frame_libre(){
	int i;

	for(i=0; i < TOTAL_FRAME_NUM; i++)
	{	
		if((BITMAP[i])) return i;
	}
	return -1;
}

int frames_needed(int size_total){
	return (size_total/PAGE_SIZE + (size_total%PAGE_SIZE != 0));
}

/*
void addr_new_segment(uint32_t size_total, uint32_t pid){
	segment* seg;
	heap_metadata* metadata1;
	int nro_pag;

	int nro_prog = search_program(pid);
	program *prog = list_get(program_list, nro_prog);
	list_add(prog->segment_table,seg);
	page* pag;
	page* pagina;

	int framesNeeded = frames_needed(size_total);

	if(number_of_free_frames()>size_total)
	{
		int nor_frame_libre = proximo_frame_libre();

		pag = page_with_free_size(PAGE_SIZE);
		nro_pag = list_add(seg->page_table, pag);

		pagina = list_get(seg->page_table, nro_pag);

		pagina->fr = MAIN_MEMORY + nor_frame_libre;

		metadata1 = (heap_metadata*) (MAIN_MEMORY + nor_frame_libre*PAGE_SIZE);
		metadata1->is_free = 1;
		metadata1-> size= size_total;

		//vamos a pedir todo el resto de paginas que necesitemos
		for(int i=0 ; i < framesNeeded - 1 ; i++ )
		{
			pag = *page_with_free_size(PAGE_SIZE);
			list_add(seg->page_table, pag);
		}
	}
}
*/

//is_first_page y totalsize son los valores que se usan en la 1ra pagina pedida para meter la metadata
//is_last_page y freesize (es el espacio libre que le queda a la última pag) la usaremos para la última metadata 
//(no, is_first_page y is_last_page no son mutuamente excluyentes)
page* page_with_free_size(int size, bool is_first_page, bool is_last_page, int totalsize, int freesize){
	heap_metadata* metadata;
	heap_metadata* metadata2;
	int espacio_libre_en_memoria = number_of_free_frames()/PAGE_SIZE;

	if (size > espacio_libre_en_memoria)
	{
		log_debug(debug_logger, "La memoria no tiene tanto espacio libre");
		return NULL;
	}

	metadata = (heap_metadata*) malloc(METADATA_SIZE);
	metadata2 = (heap_metadata*) malloc(METADATA_SIZE);

	int curr_frame_num;

	page* pag = (page *) malloc(PAGE_SIZE);

	for(curr_frame_num=0; curr_frame_num < TOTAL_FRAME_NUM; curr_frame_num++)
	{	
		if (BITMAP[curr_frame_num])
		{
			BITMAP[curr_frame_num] = 0;
			pag->is_present = true;
			pag->is_modify = false;
			pag->fr = MAIN_MEMORY + curr_frame_num * PAGE_SIZE;
			
			if (is_first_page)
			{
				metadata = (heap_metadata*) (MAIN_MEMORY + curr_frame_num * PAGE_SIZE);
				metadata->is_free = false;
				metadata->size = totalsize;
			}
			if(is_last_page)
			{
				metadata2 = (heap_metadata*) (MAIN_MEMORY + curr_frame_num * PAGE_SIZE + size);
				metadata2->is_free = true;
				metadata2->size = freesize;
			}

			return pag;
		}
	}

	log_debug(debug_logger, "Nos quedamos sin frames libres, aplicando algoritmo de reemplazo");
	return NULL;
}

//Esta funcion esta al dope
void free_page(page *pag)
{
	heap_metadata* metadata;

	for(int curr_frame_num=0; curr_frame_num < TOTAL_FRAME_NUM; curr_frame_num++)
	{	
		if (pag->fr == (MAIN_MEMORY + curr_frame_num))
		{
			BITMAP[curr_frame_num] = 0;
			metadata = (heap_metadata*) (MAIN_MEMORY + curr_frame_num * PAGE_SIZE);
			metadata->is_free = true;
			pag->is_present = true;
			pag->is_modify = false;
			pag->fr = NULL;
		}
	}	
}

void metricas_por_socket_conectado(uint32_t pid){
	int nro_prog = search_program(pid);
	program *prog = list_get(program_list, nro_prog);

	int nro_de_seg;

	int cantidad_de_segmentos_asignados = list_size(prog->segment_table);
	int cantidad_de_segmentos_totales = list_size(segment_list);

	log_trace(metricas_logger, "El programa n° %d tiene asignados %d de %d segmentos en el sistema", 
			nro_prog, cantidad_de_segmentos_asignados, cantidad_de_segmentos_totales);
	number_of_free_frames();
}

uint32_t memory_malloc(int size, uint32_t pid)
{	
	if (size <= 0) return -1;
	int nro_prog;
	uint32_t nro_seg;
	uint32_t nro_pag;
	uint32_t offset = 0;
	uint32_t logical_address = 0;
	page *pag;
	segment *seg;
	program *prog;
	int total_size = size + METADATA_SIZE;
	int total_pages_needed = (total_size / PAGE_SIZE) + ((total_size % PAGE_SIZE) != 0); // ceil(total_size / PAGE_SIZE)
	int espacio_que_queda_alocar = total_size;
	int espacio_q_quedara_libre = total_pages_needed * PAGE_SIZE - total_size;
	
	
	log_debug(debug_logger, "Se pidieron %d bytes, + metadata = %d ", size, total_size);
	
	if((nro_prog = search_program(pid)) == -1)
	{
		//si el programa no está en la lista de programas	
		//agregamos el programa a la lista de programas y le creamos un nuevo segmento

		prog = (program *) malloc(sizeof(program));		
		prog->pid = pid;
		prog->segment_table = list_create();
		nro_prog = list_add(program_list, prog);
		log_debug(debug_logger, "Se creo el prog n°%d de la lista de programas ", nro_prog);

	}	 

	prog = list_get(program_list, nro_prog);	
	
	if(segment_with_free_space(prog, size) != -1) 
	{	//si hay espacio en su segmento, lo malloqueo ahí		
		nro_seg = segment_with_free_space(prog, size);
		seg = list_get(prog->segment_table, nro_seg);	//seg = segmento con espacio

	}
	else if (ultimo_segmento_programa(prog)->is_heap && number_of_free_frames() >= total_pages_needed)
	{	//Si su ultimo segmento es heap y tiene en memoria están los frames que necesitamos
		seg = ultimo_segmento_programa(pid);
		seg->limit += total_pages_needed * PAGE_SIZE;		
		log_debug(debug_logger, "Se pudo agrandar el segmento %d ", seg);		
	}
	else 	
	{	//en ultimo caso: si no hay espacio ni se puede agrandar ningún segmento, le creo uno
		seg = (segment *) malloc(sizeof(segment));
		seg->is_heap = true;
		seg->page_table = list_create();
		seg->base = 0;
		seg->limit = total_pages_needed * PAGE_SIZE;
		nro_seg = list_add(prog->segment_table,seg);
		log_debug(debug_logger, "Se creo el segmento n°%d ", nro_seg);	
	}
	
		offset = seg->base + METADATA_SIZE;		

		if(total_pages_needed>1){
			pag = page_with_free_size(PAGE_SIZE,1,size,0,0);					
			nro_pag = list_add(seg->page_table, pag);
			espacio_que_queda_alocar =- PAGE_SIZE;

			//vamos a pedir todo el resto de paginas que necesitemos
			for(int i=0 ; i < total_pages_needed-2 ; i++ )
			{
				pag = page_with_free_size(PAGE_SIZE,0,0,0,0);
				list_add(seg->page_table, pag);
				espacio_que_queda_alocar =- PAGE_SIZE;
			}
			
			pag = page_with_free_size(espacio_que_queda_alocar,0,0,1,espacio_q_quedara_libre);					
			list_add(seg->page_table, pag);
		}
		else{ // si size_total ocupa menos de una pagina
			pag = page_with_free_size(total_size,1,size,1,espacio_q_quedara_libre);					
			nro_pag = list_add(seg->page_table, pag);
		}
	
	metricas_por_socket_conectado(pid);

	logical_address = seg->base + (nro_pag * PAGE_SIZE) + offset ;
	return logical_address;
}

int segment_with_free_space(program *prog, int size)
{
	int i=0;
	segment *segmentoActual;
	heap_metadata* metadataActual;
	heap_metadata* primerMetadata;
	int cantidadDeSegmentos = list_size(prog->segment_table);

	if (cantidadDeSegmentos == 0) return -1;

	int cantidadDePaginasDelSegmento;	

	while(i < cantidadDeSegmentos)
	{		
		if (segmentoActual->is_heap)
		{
			segmentoActual = list_get(prog->segment_table, i);

			cantidadDePaginasDelSegmento = list_size(segmentoActual->page_table);

			page* primerPagina = list_get(segmentoActual->page_table, 0);
			
			primerMetadata = (primerPagina->fr);
			metadataActual = primerMetadata;

			metadataActual = proxima_metadata_libre(0, metadataActual, 1, segmentoActual);
	
			if(metadataActual-> size >= size)
			{
				log_debug(debug_logger, "Encontramos al segmento %d con una metadata de %d de  size", 
						i, metadataActual-> size);
				return i;
			} 
		}
		i++;
	}
	log_debug(debug_logger, "No habia ningun segmento con %d de espacio libre", size);
	return -1;
}

heap_metadata* metadata_siguiente(int posicionActual, heap_metadata* metadataActual, int paginaActualNumero, segment* segmento){
	heap_metadata* metadataSiguiente;

	//Si la proxima metadata queda en otra pagina, me muevo hasta ahi
	if((posicionActual + metadataActual-> size + METADATA_SIZE) > PAGE_SIZE){
		posicionActual += posicionActual + metadataActual-> size + METADATA_SIZE;

		int cantidadDePaginasAMoverme = floor(posicionActual/PAGE_SIZE);
		int offset = (posicionActual % PAGE_SIZE);

		paginaActualNumero += cantidadDePaginasAMoverme;
		page* paginaSiguiente = list_get(segmento->page_table, paginaActualNumero);

		if (paginaSiguiente == NULL) return NULL;

		metadataSiguiente = (paginaSiguiente->fr) + offset;

	//En este caso la proxima metadata queda dentro de la misma pagina
	}else{
		posicionActual = metadataActual-> size + METADATA_SIZE;

		page* paginaActual = list_get(segmento->page_table, paginaActualNumero);

		metadataSiguiente = (paginaActual->fr) + posicionActual;
	}

	return metadataSiguiente;
}

/* Se fija si la siguiente metadata is_free y sino */ 
heap_metadata* proxima_metadata_libre(int posicionActual, heap_metadata* metadataUsada, int paginaActualNumero, segment* segmento){
	heap_metadata* metadataSiguiente;

	metadataSiguiente = metadata_siguiente(posicionActual, metadataUsada, paginaActualNumero, segmento);

	if(metadataSiguiente->is_free){
		return metadataSiguiente;
	}else if(metadataSiguiente == NULL)
		return NULL;
	else{
		return proxima_metadata_libre(posicionActual, metadataSiguiente, paginaActualNumero, segmento);
	}
}

void compactar_en_segmento(int posicionActual, heap_metadata* metadataInicial, heap_metadata* metadataActual, int paginaActualNumero, segment* segmento){
	heap_metadata* metadataSiguiente = metadata_siguiente(posicionActual, metadataActual, paginaActualNumero, segmento);

	if(metadataSiguiente->is_free){
		metadataInicial-> size += metadataSiguiente-> size;
		metadataActual = metadataSiguiente;
		compactar_en_segmento(posicionActual, metadataInicial, metadataActual, paginaActualNumero, segmento);
	}else{
		metadataInicial = proxima_metadata_libre(posicionActual, metadataSiguiente, paginaActualNumero, segmento);
		metadataActual = metadataInicial;
		compactar_en_segmento(posicionActual, metadataInicial, metadataActual, paginaActualNumero, segmento);
	}
}

void compactar_espacios_libres(program *prog){
	int cantidad_de_segmentos = list_size(prog->segment_table);
	int posicionActual = 0;
	int paginaActualNumero = 0;
	int cantidadDePaginasAMoverme;
	int i;
	heap_metadata* metadataActual;
	heap_metadata* metadataSiguiente;

	for(i=0; i < cantidad_de_segmentos; i++){
		segment* segmentoActual = list_get(prog->segment_table, i);

		int cantidadDePaginasDelSegmento = list_size(segmentoActual->page_table);

		page* primerPagina = list_get(segmentoActual->page_table, 0);

		heap_metadata* primerMetadata = (primerPagina->fr);
		metadataActual = primerMetadata;

		if(!primerMetadata->is_free){
			metadataActual = proxima_metadata_libre(posicionActual, primerMetadata, 0, segmentoActual);
		}

		compactar_en_segmento(posicionActual, metadataActual, metadataActual, 0, segmentoActual);
	}

}

uint8_t memory_free(uint32_t virtual_address, uint32_t pid)
{
	int nro_prog = search_program(pid);
	program *prog;
	segment *seg;
	page *pag;	
	heap_metadata* metadata;

	prog = list_get(program_list, nro_prog);
	int nro_seg = busca_segmento(prog,virtual_address);	
	seg = list_get(prog->segment_table,nro_seg);
	int base = seg->base;
	int numero_pagina = (virtual_address - base) / PAGE_SIZE;
	pag = list_get(seg->page_table,numero_pagina);
	int offset = (virtual_address - base) - (numero_pagina * PAGE_SIZE);
	printf("%d representa el seg %d pag %d offset %d \n",virtual_address,nro_seg,numero_pagina,offset);
	
	//free_page(pag);

	metadata = (pag->fr) + offset - METADATA_SIZE;
	metadata->is_free=1;

	compactar_espacios_libres(prog);

	return 0;
}

segment* ultimo_segmento_programa(program *prog){
	segment* seg;;
	int ultimo_elemento = list_size(prog->segment_table)-1;
	seg = list_get(prog->segment_table, ultimo_elemento );
	return seg;
}

void crear_nuevo_segmento_mmap(size_t length, void* map, uint32_t pid){
	segment* seg;
	page* pag;
	seg = (segment *) malloc(sizeof(segment));
	seg->is_heap = false;
	seg->page_table = list_create();
	//seg->base = (ultimo_segmento_programa(pid)->limit+1);
	seg->limit = &(seg->base) + length;

	int nro_pag;
	int total_pages_needed;

	total_pages_needed = (length / PAGE_SIZE) + ((length % PAGE_SIZE) != 0); // ceil(length / PAGE_SIZE);

		//vamos a pedir todo el resto de paginas que necesitemos
		for(int i=0 ; i < total_pages_needed-1 ; i++ )
		{
			pag = (page *) malloc(sizeof(page));
			pag->is_modify = false;
			pag->is_present = false;
			pag->fr = map;
			//Y que haces con esto despues? denuevo TODO
			nro_pag = list_add(seg->page_table, pag);
		}

}

int busca_segmento(program *prog, uint32_t va)
{
	int i=0;
	segment *seg;
	int cantidadDeSegmentos = list_size(prog->segment_table);

	if (cantidadDeSegmentos==0) return -1;

	uint32_t virtual_address_base = 0;
	uint32_t virtual_address_final = 0;
	
	while(i < cantidadDeSegmentos)
	{
		seg = list_get(prog->segment_table, i);	
		virtual_address_final += list_size(seg->page_table) * PAGE_SIZE;

		if(va>=virtual_address_base && va<=virtual_address_final)
		{
			printf("Se encontro el seg %d que tiene las direcciones logicas desde %d hasta %d \n",
					i, virtual_address_base, virtual_address_final);
			seg->base = virtual_address_base;		
			return i;
		} 
		else virtual_address_base = virtual_address_final+1;
	}
	return -1;
}

uint32_t buscar_direccion_fisica(uint32_t dst, uint32_t pid){
	int i= search_program(pid);
	program *prg = list_get(program_list, i);
	segment* segmentoBuscado;

	for(int i=0; i < prg->segment_table->elements_count; i++){
		segment *seg = list_get(prg->segment_table, i);

		//habria que ver si es una direccion invalida y enviar el error
		if(dst >= seg->base && dst < seg->limit){
			segmentoBuscado = seg;
			break;
		}
	}

	int paginaBuscada = floor((dst - segmentoBuscado->base) / PAGE_SIZE);
	int offset = (dst - segmentoBuscado->base) % PAGE_SIZE;

	page* paginaAEscribir = list_get(segmentoBuscado->page_table, paginaBuscada);

	uint32_t direccionFisicaBuscada = (paginaAEscribir->fr) + offset;

	return direccionFisicaBuscada;
}
// Copia n bytes de MUSE a LIBMUSE
// No se usan ni dst ni src
uint32_t memory_get(void *dst, uint32_t src, size_t numBytes, uint32_t pid)
{
	int direccionFisicaBuscada = buscar_direccion_fisica(src, pid);
	memcpy(dst, direccionFisicaBuscada, numBytes);

	return src;

}

//Copia n bytes de LIBMUSE a MUSE
uint32_t memory_cpy(uint32_t dst, void *src, int n, uint32_t pid)
{
	int direccionFisicaBuscada = buscar_direccion_fisica(dst, pid);
	memcpy(direccionFisicaBuscada, src, n);
	
	return dst;
}


// Apalancándonos en el mismo mecanismo que permite el swapping de páginas,
// la funcionalidad de memoria compartida que proveerá MUSE (a través de sus 
// funciones de muse_map) se realizará sobre un archivo compartido, en vez 
// del archivo de swap. Esta distinción deberá estar plasmada en la tabla de segmentos.
uint32_t memory_map(char *path, size_t length, int flags, uint32_t pid)
{
	/* Idea Original:*/
	int file_descriptor = open(path, O_RDONLY);
   	void* map = mmap(NULL, length, PROT_NONE, flags, file_descriptor, 0);
   	printf(map, length);

	crear_nuevo_segmento_mmap(length,map,pid);   

	return *(int*) map; 

	// How does mmap works:
	// mmap() creates a new mapping in the virtual address space of the calling process.
	// addr specifies address for the file, length specifies the length of the mapping 
	// void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);   
}

/* mapea el archivo swap integramente */
char* get_file_content_from_swap(){
	char* fileMapeado;
	int fileDescriptor;
	struct stat estadoDelFile; //declaro una estructura que guarda el estado de un archivo

	char* swapfile;

	fileDescriptor = open(swapfile, O_RDWR);
		/*Chequeo de apertura del file exitosa*/
			if (fileDescriptor==-1){
				log_error(debug_logger,"Fallo la apertura del file de datos");
				exit(-1);
			}
	if(fstat(fileDescriptor,&estadoDelFile)==-1){//guardo el estado del archivo de datos en la estructura
			log_error(debug_logger,"Fallo el fstat");
			exit(-1);
		}
	fileMapeado = mmap(0,estadoDelFile.st_size,(PROT_WRITE|PROT_READ|PROT_EXEC),MAP_SHARED,fileDescriptor,0);
	/*Chequeo de mmap exitoso*/
		if (fileMapeado==MAP_FAILED){
			log_error(debug_logger,"Fallo el mmap, no se pudo asignar la direccion de memoria para el archivo solicitado");
			exit(-1);
		}
	close(fileDescriptor); //Cierro el archivo

	return fileMapeado;
}

uint32_t memory_sync(uint32_t addr, size_t len, uint32_t pid)
{
	/*synchronize a file with a memory map*/
	//msync(addr, len, MS_SYNC);
	return 0;
}



int memory_unmap(uint32_t dir, uint32_t pid)
{
	/* Idea Original:*/
	void *dir2 = &dir;
	int unmap_result = munmap(dir2,  1 << 10); //TODO: ¿¿ 1 << 10 ??
  	if (unmap_result == 0 ) {
		printf("Could not unmap");
		//log_error(muse_logger,"Could not unmap");
		return -1;
	}
	return 0;
}
