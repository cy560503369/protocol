#include <stdio.h>
#include <string.h>
#include <unistd.h>
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
	char* test_data = "{\"command\": \"protocol_config_set\",\"message\": {\"protocol\": \"iec103\",\"work_mode\": \"poll\",\"port\": \"/dev/ttyS1\",\"classify\": [{\"device_addr\": [\"1\", \"2\"],\"state_table\": [[178,20,\"id_ps\"],[178,23,\"id_fss\"],[178,48,\"id_fo\"]],\"message_table\": [{\"group\": 9,\"setting\": [[2, \"id_angia\"],[14, \"id_anguc\"]]}]}]}}";
	char* a104_test = "{\"command\": \"protocol_config_set\",\"message\": {\"classify\": [{\"message_table\": [{\"group\": 1,\"setting\": [[1024,\"SHBIOT.TEST.TEST04\"],[1025,\"SHBIOT.TEST.TEST05\"]]}],\"device_addr\": [\"1\"],\"state_table\": [[1,1,\"SHBIOT.TEST.TEST03\"]]}],\"protocol\": \"iec104\",\"port\": \"2404\",\"work_mode\": \"192.168.31.224\"}}";
	char* a102_test = "{\"command\": \"protocol_config_set\",\"message\": {\"classify\": [{\"message_table\": [{\"group\": 42,\"setting\": [[2,\"SHBIOT.TEST.TEST04\"],[5,\"SHBIOT.TEST.TEST05\"]]}],\"device_addr\": [\"1\"],\"state_table\": [[51,2,\"id_status\"]]}],\"protocol\": \"iec102\",\"port\": \"/dev/ttyS2\",\"work_mode\": \"poll\"}}";
	pthread_rwlockattr_t attr;

	Protocol_config_sm* p_conf = build_shared_memory(PROTOCOL103_CONFIG_SM_KEY, PROTOCOL103_CONFIG_LEN);
	Protocol_data_sm* p_data = build_shared_memory(PROTOCOL103_DATA_SM_KEY, PROTOCOL103_DATA_LEN);

	Protocol_config_sm* p104_conf = build_shared_memory(PROTOCOL104_CONFIG_SM_KEY, PROTOCOL104_CONFIG_LEN);
	Protocol_data_sm* p104_data = build_shared_memory(PROTOCOL104_DATA_SM_KEY, PROTOCOL104_DATA_LEN);

	Protocol_config_sm* p102_conf = build_shared_memory(PROTOCOL102_CONFIG_SM_KEY, PROTOCOL102_CONFIG_LEN);
	Protocol_data_sm* p102_data = build_shared_memory(PROTOCOL102_DATA_SM_KEY, PROTOCOL102_DATA_LEN);

	pthread_rwlockattr_init(&attr);
	pthread_rwlockattr_setpshared(&attr,PTHREAD_PROCESS_SHARED);
	pthread_rwlock_init(&p_conf->rwlock, &attr);
	pthread_rwlock_init(&p_data->rwlock, &attr);
	pthread_rwlock_init(&p104_conf->rwlock, &attr);
	pthread_rwlock_init(&p104_data->rwlock, &attr);
	pthread_rwlock_init(&p102_conf->rwlock, &attr);
	pthread_rwlock_init(&p102_data->rwlock, &attr);
	pthread_rwlockattr_destroy(&attr);


	pthread_rwlock_wrlock(&p_conf->rwlock);
	p_conf->update_flag = 1;
	p_conf->started = 1;
	strcpy(p_conf->config_data, test_data);
	pthread_rwlock_unlock(&p_conf->rwlock);

	pthread_rwlock_wrlock(&p104_conf->rwlock);
	p104_conf->update_flag = 1;
	p104_conf->started = 1;
	strcpy(p104_conf->config_data, a104_test);
	pthread_rwlock_unlock(&p104_conf->rwlock);

	pthread_rwlock_wrlock(&p102_conf->rwlock);
	p102_conf->update_flag = 1;
	p102_conf->started = 0;
	strcpy(p102_conf->config_data, a102_test);
	pthread_rwlock_unlock(&p102_conf->rwlock);

	while(1)
	{
		printf("103data:\n");
		pthread_rwlock_wrlock(&p_data->rwlock);
		printf("%s\n", p_data->protocol_data);
		pthread_rwlock_unlock(&p_data->rwlock);

		printf("104data:\n");
		pthread_rwlock_wrlock(&p104_data->rwlock);
		printf("%s\n", p104_data->protocol_data);
		pthread_rwlock_unlock(&p104_data->rwlock);
		
		sleep(5);
	}
	return 0;
}
