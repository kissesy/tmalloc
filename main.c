#include "malloc.h"

int main(int argc, char** argv){
	char* a = (char*)tmalloc(0x100);
	printf("%p \n",a);
	char* b = (char*)tmalloc(0x100);
	printf("%p \n",b);
	char* c = (char*)tmalloc(0x100);
	printf("%p \n",c);
	tfree(a);
	tfree(b);
	char* d = (char*)tmalloc(0x210);
	printf("%p \n",d);
	tfree(d);
	return 0;
}
