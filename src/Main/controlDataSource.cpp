//-----------------------------------------------------------------------------
// Copyright (c) 2020 Chris Calef
//-----------------------------------------------------------------------------

#include "controlDataSource.h"
#include <simgear\debug\logstream.hxx>

extern void  exitProgramTemp();
extern DWORD returnTimeOfDay();
extern void  doEngineStart();
extern void  doAircraftReset();

controlDataSource::controlDataSource(bool listening, bool alternating, int port, char* IP) : dataSource(listening, alternating, port, IP)
{
}

controlDataSource::~controlDataSource()
{
}

////////////////////////////////////////////////////////////////////////////////
void controlDataSource::tick()
{
    char message[1024];
    //sprintf(message, "controlDataSource tick %d server stage %d client stage %d sendcontrols %d time %d", mCurrentTick, mServerStage, mClientStage, mSendControls, returnTimeOfDay());
    //SG_LOG(SG_GENERAL, SG_INFO, message);
    if (mSendControls > 0) {
        //sprintf(message, "controlDataSource sending, sendControls %d  clientStage %d  ", mSendControls, mClientStage);
        //SG_LOG(SG_GENERAL, SG_INFO, message);
        if (mClientStage == ClientSocketConnected)
            sendPacket();
        else
            connectSendSocket();
    } else if (mListening) {
        //sprintf(message, "controlDataSource listening, serverStage %d", mServerStage);
        //SG_LOG(SG_GENERAL, SG_INFO, message);
        if (mServerStage == ServerSocketAccepted)
            readPacket();
        else
            connectListenSocket();
    }
    mCurrentTick++;
}


/*
    if (mListening) {
        //sprintf(message, "controlDataSource tick %d server stage %d time %d", mCurrentTick, mServerStage, returnTimeOfDay());
        //SG_LOG(SG_GENERAL, SG_INFO, message);
        switch (mServerStage) {
        case NoServerSocket:
            createListenSocket();
            break;
        case ServerSocketCreated:
            bindListenSocket();
            break;
        case ServerSocketBound:
            connectListenSocket();
            break;
        case ServerSocketListening:
            listenForConnection();
            break;
        case ServerSocketAccepted:
            readPacket();
            break;
            //case PacketReceived:
            //    readPacket();
            //    break;
            //case PacketRead:
            //    mServerStage = ServerSocketListening;
            //    break;
        }
    } else {
        //sprintf(message, "controlDataSource tick %d client stage %d time %d", mCurrentTick, mClientStage, returnTimeOfDay());
        //SG_LOG(SG_GENERAL, SG_INFO, message);
        switch (mClientStage) {
        case NoClientSocket:
            connectSendSocket();
            break;
        case ClientSocketCreated:
            break;
        case ClientSocketConnected:
            addBaseRequest();
            //addTestRequest();
            sendPacket();
            break;
        }
    }
    mCurrentTick++;
    */

void controlDataSource::readPacket()
{ 
    FD_SET  ReadSet;
    DWORD   total;
    timeval tval;
    tval.tv_sec  = 0;
    tval.tv_usec = 30;

    FD_ZERO(&ReadSet);
    FD_SET(mWorkSockfd, &ReadSet);

    //THIS is how you actually enforce non-blocking status! Recv will just wait, on its own.
    total = select(0, &ReadSet, NULL, NULL, &tval);
    if (total == 0) {
        //SG_LOG(SG_GENERAL, SG_INFO, "readPacket bailed on select");
        return;
    }
    
    short opcode, controlCount;
    char  message[1024];

    int n = recv(mWorkSockfd, mReturnBuffer, mPacketSize, 0); //mWorkSockfd
    if (n < 0) {
        //SG_LOG(SG_GENERAL, SG_INFO, "readPacket bailed on recv");
        return;
    }

    controlCount = readShort();
    for (short i = 0; i < controlCount; i++) {
        opcode = readShort();

        sprintf(message, "controlDataSource Reading packet, opcode = %d time %d", opcode, returnTimeOfDay());
        SG_LOG(SG_GENERAL, SG_INFO, message);
        if (mDebugToConsole)
            std::cout << "Reading packet, opcode = " << opcode << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "Reading packet, opcode = %d \n\n", opcode);

        if (opcode == OPCODE_BASE) {
            handleBaseRequest();
        } else if (opcode == OPCODE_TEST) {
            handleTestRequest();
        } else if (opcode == OPCODE_QUIT) {
            handleQuitRequest();
        } else if (opcode == OPCODE_ENGINE_START) {
            handleEngineStartRequest();
        } else if (opcode == OPCODE_RESET) {
            handleResetRequest();
        }
    }
    clearReturnPacket();
    //mServerStage = PacketRead;
    //mServerStage = ServerSocketAccepted;
    //if (mAlternating)
    //    mListening = false;
}

void controlDataSource::addQuitRequest()
{
    short opcode = OPCODE_QUIT;
    mSendControls++;
    writeShort(opcode);
    //For a baseRequest, do nothing but send a tick value to make sure there's a connection.
}

void controlDataSource::handleQuitRequest()
{
    char message[1024];
    sprintf(message, "dataSource - handleQuitRequest");
    SG_LOG(SG_GENERAL, SG_INFO, message);
    if (mDebugToConsole)
        std::cout << "handleQuitRequest \n";
    if (mDebugToFile)
        fprintf(mDebugLog, "handleQuitRequest\n\n");

    exitProgramTemp();
}


void controlDataSource::addEngineStartRequest()
{
    short opcode = OPCODE_ENGINE_START;
    mSendControls++;
    writeShort(opcode);
}

void controlDataSource::handleEngineStartRequest()
{
    char message[1024];
    sprintf(message, "dataSource - handleEngineStartRequest");
    SG_LOG(SG_GENERAL, SG_INFO, message);

    doEngineStart();
}

void controlDataSource::addResetRequest()
{
    short opcode = OPCODE_RESET;
    mSendControls++;
    writeShort(opcode);
}

void controlDataSource::handleResetRequest()
{
    char message[1024];
    sprintf(message, "dataSource - handleResetRequest");
    SG_LOG(SG_GENERAL, SG_INFO, message);

    disconnectSockets();

    doAircraftReset();
}
