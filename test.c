#include <stdio.h>
#include <string.h>
#include "protocol.h"

void* build_shared_memory(key_t key, size_t size)
{
	void* pshmbuf = NULL;
	if(size <= 0)
		return NULL;
	int shmid = shmget(key,size, IPC_CREAT|SHM_R|SHM_W|IPC_EXCL);
	if(shmid < 0)
	{
		shmid = shmget(key,0,SHM_R|SHM_W);
		
		if(shmid < 0)
		{
			fprintf(stderr,"create share memory %u failed\n",key);
			return NULL;
		}
		else
		{
			if(shmctl(shmid,IPC_RMID,0)<0)
				return NULL;
			else 
			{
				shmid = shmget(key,size, IPC_CREAT|SHM_R|SHM_W|IPC_EXCL);
				if(shmid < 0)
					return NULL;
			}
		}
	}
	pshmbuf = (char *)shmat(shmid,0,0);	
	
	return pshmbuf;
}

int main(void)
{
	char* test_data = "{\"command\": \"protocol_config_set\",\"message\": {\"protocol\": \"iec103\",\"work_mode\": \"poll\",\"port\": \"/dev/ttyS1\",\"classify\": [{\"device_addr\": [\"1\"],\"state_table\": [[178,20,\"id_ps\"],[178,23,\"id_fss\"],[178,48,\"id_fo\"]],\"message_table\": [{\"group\": 9,\"setting\": [[2, \"id_angia\"],[14, \"id_anguc\"]]}]}]}}";
	pthread_rwlockattr_t attr;

	Protocol_config_sm* p_conf = build_shared_memory(PROTOCOL103_CONFIG_SM_KEY, 4*1024);

	pthread_rwlockattr_init(&attr);
	pthread_rwlockattr_setpshared(&attr,PTHREAD_PROCESS_SHARED);
	pthread_rwlock_init(&p_conf->rwlock, &attr);
	pthread_rwlockattr_destroy(&attr);


	pthread_rwlock_wrlock(&p_conf->rwlock);
	p_conf->update_flag = 1;
	p_conf->started = 1;
	strcpy(p_conf->config_data, test_data);
	pthread_rwlock_unlock(&p_conf->rwlock);
	return 0;
}
