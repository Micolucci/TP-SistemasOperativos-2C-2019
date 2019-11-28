#include "libmuse.h"
#include <unistd.h>

int main(int argc,char *argv[])
{
	int pid1 = getpid();
	if (muse_init(pid1) == 0)
		printf("Libmuse with proccess_id = %d initialized!\n",pid1);


	// int pi2 = muse_init(21);	
	// printf("Libmuse with proccess_id = %d initialized!\n",pi2);

	int dir = muse_alloc(200);
	printf("La direccion de los 200 byes reservados es %d\n",dir);
	int dir2 = muse_alloc(300);
	printf("La direccion de los 300 byes reservados es %d\n",dir2);
	int dir3 = muse_alloc(500);
	printf("La direccion de los 500 byes reservados es %d\n",dir3);

	muse_free(dir);
	printf("Se libero la direccion logica %d\n",dir);

	muse_close();

	int pid2 = getpid();
	if (muse_init(pid2) == 0)
		printf("Libmuse with proccess_id = %d initialized!\n",pid2);
	
	int dir4 = muse_alloc(2000);
	printf("La direccion de los 2000 byes reservados es %d\n",dir4);
	

	return 0;
}
