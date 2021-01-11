#include "cs104_connection.h"
#include "hal_time.h"
#include "hal_thread.h"
#include "protocol.h"
#include "protocol104.h"

#include <stdio.h>
#include <stdlib.h>

Procotol104_data p104_data[1000] = {0};
unsigned int p104_data_post = 0;

/* Callback handler to log sent or received messages (optional) */
static void rawMessageHandler (void* parameter, uint8_t* msg, int msgSize, bool sent)
{
    if (sent)
        printf("SEND: ");
    else
        printf("RCVD: ");

    int i;
    for (i = 0; i < msgSize; i++) {
        printf("%02x ", msg[i]);
    }

    printf("\n");
}

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
    printf("RECVD ASDU type: %s(%i) elements: %i\n",
            TypeID_toString(CS101_ASDU_getTypeID(asdu)),
            CS101_ASDU_getTypeID(asdu),
            CS101_ASDU_getNumberOfElements(asdu));

    if (CS101_ASDU_getTypeID(asdu) == M_ME_TE_1) {

        printf("  measured scaled values with CP56Time2a timestamp:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            MeasuredValueScaledWithCP56Time2a io =
                    (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu, i);

            printf("    IOA: %i value: %i\n",
                    InformationObject_getObjectAddress((InformationObject) io),
                    MeasuredValueScaled_getValue((MeasuredValueScaled) io)
            );

            MeasuredValueScaledWithCP56Time2a_destroy(io);
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_SP_NA_1) {
        printf("  single point information:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            SinglePointInformation io =
                    (SinglePointInformation) CS101_ASDU_getElement(asdu, i);

            printf("    IOA: %i value: %i\n",
                    InformationObject_getObjectAddress((InformationObject) io),
                    SinglePointInformation_getValue((SinglePointInformation) io)
            );

            SinglePointInformation_destroy(io);
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_ME_ND_1) {
        printf("  measured point information:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            MeasuredValueNormalizedWithoutQuality io = 
                    (MeasuredValueNormalizedWithoutQuality) CS101_ASDU_getElement(asdu, i);

            printf("    IOA: %i value: %lf\n",
                    InformationObject_getObjectAddress((InformationObject) io),
                    MeasuredValueNormalizedWithoutQuality_getValue((MeasuredValueNormalizedWithoutQuality) io)
            );

            MeasuredValueNormalizedWithoutQuality_destroy(io);
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_IT_NA_1) {
        printf("  measured point information:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            IntegratedTotals io = (IntegratedTotals) CS101_ASDU_getElement(asdu, i);

            BinaryCounterReading covf = IntegratedTotals_getBCR((IntegratedTotals) io);

            printf("    IOA: %i, Value: %i\n",
                    InformationObject_getObjectAddress((InformationObject) io),
                    BinaryCounterReading_getValue(covf)
            );

            IntegratedTotals_destroy(io);
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == C_TS_TA_1) {
        printf("  test command with timestamp\n");
    }

    return true;
}

void protocol104_main(void)
{
    const char* ip = "192.168.31.224";
    uint16_t port = IEC_60870_5_104_DEFAULT_PORT;

    printf("Connecting to: %s:%i\n", ip, port);
    CS104_Connection con = CS104_Connection_create(ip, port);

    CS101_AppLayerParameters alParams = CS104_Connection_getAppLayerParameters(con);
    alParams->originatorAddress = 3;

    CS104_Connection_setConnectionHandler(con, connectionHandler, NULL);
    CS104_Connection_setASDUReceivedHandler(con, asduReceivedHandler, NULL);

    /* uncomment to log messages */
    //CS104_Connection_setRawMessageHandler(con, rawMessageHandler, NULL);

    if (CS104_Connection_connect(con)) {
        printf("Connected!\n");

        CS104_Connection_sendStartDT(con);

        Thread_sleep(2000);

        CS104_Connection_sendInterrogationCommand(con, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);

        Thread_sleep(5000);

        CS104_Connection_sendCounterInterrogationCommand(con, CS101_COT_ACTIVATION, 0, 0);
        Thread_sleep(5000);
        // struct sCP56Time2a testTimestamp;
        // CP56Time2a_createFromMsTimestamp(&testTimestamp, Hal_getTimeInMs());

        // CS104_Connection_sendTestCommandWithTimestamp(con, 1, 0x4938, &testTimestamp);

#if 0
        InformationObject sc = (InformationObject)
                SingleCommand_create(NULL, 5000, true, false, 0);

        printf("Send control command C_SC_NA_1\n");
        CS104_Connection_sendProcessCommandEx(con, CS101_COT_ACTIVATION, 1, sc);

        InformationObject_destroy(sc);

        /* Send clock synchronization command */
        struct sCP56Time2a newTime;

        CP56Time2a_createFromMsTimestamp(&newTime, Hal_getTimeInMs());

        printf("Send time sync command\n");
        CS104_Connection_sendClockSyncCommand(con, 1, &newTime);
#endif

        printf("Wait ...\n");

        Thread_sleep(1000);
    }
    else
        printf("Connect failed!\n");

    Thread_sleep(1000);

    CS104_Connection_destroy(con);

    printf("exit\n");
}


