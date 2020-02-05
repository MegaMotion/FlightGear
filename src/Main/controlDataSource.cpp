//-----------------------------------------------------------------------------
// Copyright (c) 2015 Chris Calef
//-----------------------------------------------------------------------------

#include "controlDataSource.h"
#include <simgear\debug\logstream.hxx>

extern void exitProgramTemp();


controlDataSource::controlDataSource(bool listening, bool alternating, int port, char *IP) : dataSource(listening, alternating, port, IP)
{

}

controlDataSource::~controlDataSource()
{

}

////////////////////////////////////////////////////////////////////////////////
void controlDataSource::tick()
{
	if (mListening)
	{
		//Con::printf("controlDataSource tick %d server stage %d", mCurrentTick, mServerStage);
		switch (mServerStage)
		{
		case NoServerSocket:
			createListenSocket(); break;
		case ServerSocketCreated:
			bindListenSocket(); break;
		case ServerSocketBound:
			connectListenSocket(); break;
		case ServerSocketListening:
			listenForConnection(); break;
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
	}
	else
	{
		//Con::printf("controlDataSource tick %d client stage %d", mCurrentTick, mClientStage);
		switch (mClientStage)
		{
		case NoClientSocket:
			connectSendSocket(); break;
		case ClientSocketCreated:
			break;
		case ClientSocketConnected:
			addBaseRequest();
			addTestRequest();
			sendPacket();
			break;
		}
	}
	mCurrentTick++;
}


void controlDataSource::readPacket()
{
    short opcode, controlCount; //,packetCount;

    int n = recv(mWorkSockfd, mReturnBuffer, mPacketSize, 0); //mWorkSockfd
    if (n < 0)
        return;

    controlCount = readShort();
    for (short i = 0; i < controlCount; i++) {
        opcode = readShort();

        char message[1024];
        sprintf(message, "controlDataSource Reading packet, opcode = %d", opcode);
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
        }
    }
    clearReturnPacket();
    //mServerStage = PacketRead;
    mServerStage = ServerSocketAccepted;
    if (mAlternating)
        mListening = false;
}

void controlDataSource::addQuitRequest()
{
	short opcode = OPCODE_QUIT;
	mSendControls++;//(Increment this every time you add a control.)
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
