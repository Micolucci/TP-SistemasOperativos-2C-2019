#include "libmuse.h"

int main(int argc,char *argv[])
{
	libmuse_init("127.0.0.1",5003);
	printf("Libmuse initialized!\n");
	printf("Comencing test!\n");
	sleep(2);
	say("hola muse\n");
	printf("Message sent!\n");
	printf("Press a button to continue\n");
	getchar();
	
	return 0;
}
