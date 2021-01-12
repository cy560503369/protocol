#include "cs104_connection.h"
#include "hal_time.h"
#include "hal_thread.h"
#include "protocol.h"
#include "protocol104.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Procotol104_data p104_data[1000] = {0};    // 104数据缓冲区
unsigned int p104_data_post = 0;    // 当前数据位置

unsigned char connec_flag = 0;      // 104连接状态，0，未连接，1、已连接

/* Connection event handler */
static void connectionHandler (void* parameter, CS104_Connection connection, CS104_ConnectionEvent event)
{
    switch (event) {
    case CS104_CONNECTION_OPENED:
        printf("Connection established\n");
        break;
    case CS104_CONNECTION_CLOSED:
        printf("Connection closed\n");
        break;
    case CS104_CONNECTION_STARTDT_CON_RECEIVED:
        printf("Received STARTDT_CON\n");
        break;
    case CS104_CONNECTION_STOPDT_CON_RECEIVED:
        printf("Received STOPDT_CON\n");
        break;
    }
}

/*
 * CS101_ASDUReceivedHandler implementation
 *
 * For CS104 the address parameter has to be ignored
 */
static bool asduReceivedHandler (void* parameter, int address, CS101_ASDU asdu)
{
    if (CS101_ASDU_getTypeID(asdu) == M_ME_TE_1) 
    {
        int i;
        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) 
        {

            MeasuredValueScaledWithCP56Time2a io =
                    (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu, i);

            p104_data[p104_data_post].point_addr = InformationObject_getObjectAddress((InformationObject) io);
            p104_data[p104_data_post].type = TYPE_INT;
            p104_data[p104_data_post].value.i_data = MeasuredValueScaled_getValue((MeasuredValueScaled) io);
            p104_data_post++;

            MeasuredValueScaledWithCP56Time2a_destroy(io);
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_SP_NA_1) 
    {
        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) 
        {

            SinglePointInformation io =
                    (SinglePointInformation) CS101_ASDU_getElement(asdu, i);

            p104_data[p104_data_post].point_addr = InformationObject_getObjectAddress((InformationObject) io);
            p104_data[p104_data_post].type = TYPE_INT;
            p104_data[p104_data_post].value.i_data = SinglePointInformation_getValue((SinglePointInformation) io);
            p104_data_post++;

            SinglePointInformation_destroy(io);
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_ME_ND_1) 
    {
        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) 
        {

            MeasuredValueNormalizedWithoutQuality io = 
                    (MeasuredValueNormalizedWithoutQuality) CS101_ASDU_getElement(asdu, i);

            p104_data[p104_data_post].point_addr = InformationObject_getObjectAddress((InformationObject) io);
            p104_data[p104_data_post].type = TYPE_FLOAT;
            p104_data[p104_data_post].value.f_data = 
                    MeasuredValueNormalizedWithoutQuality_getValue((MeasuredValueNormalizedWithoutQuality) io);
            p104_data_post++;

            MeasuredValueNormalizedWithoutQuality_destroy(io);
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_IT_NA_1) 
    {
        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) 
        {

            IntegratedTotals io = (IntegratedTotals) CS101_ASDU_getElement(asdu, i);

            BinaryCounterReading covf = IntegratedTotals_getBCR((IntegratedTotals) io);

            p104_data[p104_data_post].point_addr = InformationObject_getObjectAddress((InformationObject) io);
            p104_data[p104_data_post].type = TYPE_INT;
            p104_data[p104_data_post].value.i_data = BinaryCounterReading_getValue(covf);
            p104_data_post++;

            IntegratedTotals_destroy(io);
        }
    }
    
    return true;
}

int parse104_config(char* config_str, Procotol104_config* out_config)
{
    int i;
    cJSON *json_root = cJSON_Parse(config_str);
    if(json_root == NULL)
    {
        return -1;
    }

    cJSON *json_item = cJSON_GetObjectItem(json_root, "message");
    if(json_item == NULL)
    {
        return -1;
    }

    cJSON* json_table = cJSON_GetObjectItem(json_item, "protocol");
    if(0 != strcmp(json_table->valuestring, "iec104"))
    {
        return -1;
    }

    json_table = cJSON_GetObjectItem(json_item, "ip"); // 获取ip
    strcpy(out_config->ip, json_table->valuestring);

    json_table = cJSON_GetObjectItem(json_item, "port"); // 获取ip
    out_config->port = json_table->valueint;

    json_table = cJSON_GetObjectItem(json_item, "classify"); 
    cJSON* json_subarray = cJSON_GetArrayItem(json_table, 0);

    cJSON* json_array = cJSON_GetObjectItem(json_subarray,"device_addr");
    int array_num = cJSON_GetArraySize(json_array);
    out_config->device_num = array_num;

    cJSON* json_data;

    for(i = 0; i < array_num; i++)
    {
        json_data = cJSON_GetArrayItem(json_array, i);
        out_config->device_addr[i] = (unsigned char)(json_data->valuestring[0] - '0');
    }

    json_array = cJSON_GetObjectItem(json_subarray,"catch_table");
    array_num = cJSON_GetArraySize(json_array);
    out_config->catch_num = array_num;

    cJSON *catch_data, *arr_data;
    for(i = 0; i < array_num; i++)
    {
        catch_data = cJSON_GetArrayItem(json_array, i);
        arr_data = cJSON_GetArrayItem(catch_data, 0);
        out_config->catch_list[i].point_addr = arr_data->valueint;

        arr_data = cJSON_GetArrayItem(catch_data, 1);
        strcpy(out_config->catch_list[i].point_name, arr_data->valuestring);
    }

    cJSON_Delete(json_root);
    return 0;
}

void init_104_data(void)
{
    p104_data_post = 0;
    memset(p104_data, 0, sizeof(Procotol104_data) * 1000);
}

/* 生成JSON数据，并存入共享内存*/
int make_104_data(Procotol104_config* conf, Protocol_data_sm* data_sm)
{
    time_t t;
    t = time(NULL);
    int now_time = time(&t);

    int i = 0, j = 0;

    if(p104_data_post == 0)
    {
        return 0;
    }

    cJSON* cjson_array = cJSON_CreateArray();
    cJSON* cjson_item = NULL;

    for(i = 0; i < conf->catch_num; i++)
    {
        cjson_item = cJSON_CreateObject();
        cJSON_AddStringToObject(cjson_item, "id", conf->catch_list[i].point_name);

        for(j = 0; j < p104_data_post; j++)
        {
            if(conf->catch_list[i].point_addr == p104_data[j].point_addr)
            {
                if(p104_data[j].type == TYPE_INT)
                {
                    cJSON_AddNumberToObject(cjson_item, "value", p104_data[j].value.i_data);
                }
                else
                {
                    cJSON_AddNumberToObject(cjson_item, "value", p104_data[j].value.f_data);
                }
                break;
            }
        }

        if(j >= p104_data_post)
        {
            cJSON_AddNumberToObject(cjson_item, "value", 0);
        }

        cJSON_AddNumberToObject(cjson_item, "time", now_time);
        cJSON_AddItemToArray(cjson_array, cjson_item);
    }

    char* str = cJSON_Print(cjson_array);

    pthread_rwlock_wrlock(&data_sm->rwlock);
    memset(data_sm->protocol_data, 0, PROTOCOL104_DATA_LEN);  // 清空数据区
    strcpy(data_sm->protocol_data, str);    // 将结果写入数据区
    pthread_rwlock_unlock(&data_sm->rwlock);

    cJSON_Delete(cjson_array);
    return 0;
}

void protocol104_main(void)
{
    /* 挂接配置文件的共享内存 */
    Protocol_config_sm* p104_config_sm = get_shared_memory(PROTOCOL104_CONFIG_SM_KEY);
    /* 挂接数据文件的共享内存 */
    Protocol_data_sm* p104_data_sm = get_shared_memory(PROTOCOL104_DATA_SM_KEY);

    Procotol104_config p104_conf = {0};

    while(1)
    {
        // 处理配置文件
        pthread_rwlock_wrlock(&p104_config_sm->rwlock);
        if(p104_config_sm->started != 1)   // 判断启动标志位，如果不是为启动，直接退出
        {
            pthread_rwlock_unlock(&p104_config_sm->rwlock);
            sleep(10);
            return;
        }
        parse104_config(p104_config_sm->config_data, &p104_conf);
        pthread_rwlock_unlock(&p104_config_sm->rwlock);

        init_104_data();  // 清空数据缓冲区

        CS104_Connection con = CS104_Connection_create(p104_conf.ip, p104_conf.port);

        CS101_AppLayerParameters alParams = CS104_Connection_getAppLayerParameters(con);
        alParams->originatorAddress = 3;

        CS104_Connection_setConnectionHandler(con, connectionHandler, NULL);
        CS104_Connection_setASDUReceivedHandler(con, asduReceivedHandler, NULL);

        if (CS104_Connection_connect(con)) {

            CS104_Connection_sendStartDT(con);

            Thread_sleep(1000);

            CS104_Connection_sendInterrogationCommand(con, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);

            Thread_sleep(1000);

            CS104_Connection_sendCounterInterrogationCommand(con, CS101_COT_ACTIVATION, 0, 0);
            Thread_sleep(1000);

            CS104_Connection_sendStopDT(con);
            Thread_sleep(2000);
        }

        Thread_sleep(1000);

        CS104_Connection_destroy(con);

        make_104_data(&p104_conf, p104_data_sm);

        sleep(10);
    }
}


