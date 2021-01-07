
#include <time.h>
#include "protocol103.h"
#include "protocol.h"


/* 挂接共享，共享内存需要在整个系统初始化时进行创建，
	该处只是对共享进行使用 */
void* get_shared_memory(key_t key)
{
	void* pshmbuf = NULL;
	int shmid = 0;
	
	shmid = shmget(key,0, SHM_R|SHM_W);
	if(shmid >= 0)
	{
		pshmbuf = (char *)shmat(shmid,0,0);
		return pshmbuf;
	}
	return NULL;
}

int main(void)
{
	protocol103_main();
}