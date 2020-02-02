//-----------------------------------------------------------------------------
// Copyright (c) 2020 Chris Calef
//-----------------------------------------------------------------------------

#include <simgear\debug\logstream.hxx>
#include "dataSource.h"

//Nope! Including any of these causes the bind conflict!
//#include <src\Main\fg_props.hxx>
//#include <src\Main\fg_os.hxx>
//#include <src\Main\fg_init.cxx>

//using namespace std;//Or, maybe not, possible name conflicts.

WSADATA wsaData;

dataSource::dataSource(bool server, int port, char* IP)
{
    mPort = port;
    sprintf(mSourceIP, IP);
    mPacketSize        = 1024;
    mCurrentTick       = 0;
    mLastSendTick      = 0;
    mLastSendTimeMS    = 0;
    mTickInterval      = 1; //For skipping ticks, in case they are getting called faster than desired.
    mStartDelayMS      = 1000;
    mPacketCount       = 0;
    mMaxPackets        = 20;
    mSendControls      = 0;
    mReturnByteCounter = 0;
    mSendByteCounter   = 0;

    mListenSockfd = 0;
    mWorkSockfd   = 0;

    mReturnBuffer     = NULL;
    mSendBuffer       = NULL;
    mStringBuffer     = NULL;
    mReadyForRequests = false;
    mAlternating      = false;

    mDebugToConsole = false;
    mDebugToFile    = false;

    mServerStage = NoServerSocket;
    mClientStage = NoClientSocket;

    mServer    = false;
    mListening = false;
    if (server) {
        mServer    = true;
        mListening = true;
    }
    //printf("New data source object!!! Source IP: %s listening %d\n", mSourceIP, server);

#ifdef windows_OS //Not necessary here, as the engine has already done this. //???
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) == SOCKET_ERROR) {
        std::cout << "Error initialising WSA.\n";
    }
#endif

    if (mDebugToFile) {
        mDebugLog = fopen("log.txt", "a");
        fprintf(mDebugLog, "\n\n--------------------------- PhotoPi Debug Log\n\n");
    }
}

dataSource::~dataSource()
{
    disconnectSockets();

    if (mDebugToFile)
        fclose(mDebugLog);
}

////////////////////////////////////////////////////////////////////////////////

void dataSource::tick()
{
    //SG_LOG(SG_GENERAL, SG_INFO, "datasource tick! ");
    
    if (mServer) 
    {
        switch (mServerStage) 
        {
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
            receivePacket();
            break;
        case PacketReceived:
            readPacket();
            break;
        case PacketRead:
            mServerStage = ServerSocketListening;
            break;
        }
    } else {
        switch (mClientStage) {
        case NoClientSocket:
            connectSendSocket();
            break;
        case ClientSocketCreated:
            break;
        case ClientSocketConnected:
            addBaseRequest();
            addTestRequest();
            sendPacket();
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////

void dataSource::createListenSocket()
{
    //printf("dataSource create listen socket\n");

    SG_LOG(SG_GENERAL, SG_INFO, "dataSource - create listen socket!!!!!!");

    mListenSockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mListenSockfd < 0) {
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - ERROR in createListenSocket!!!!!");
        if (mDebugToConsole)
            std::cout << "ERROR in createListenSocket. Error: " << errno << " " << strerror(errno) << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "ERROR in createListenSocket. Error: %d %s\n", errno, strerror(errno));
    } else {
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - SUCCESS in createListenSocket!!!!!");
        if (mDebugToConsole)
            std::cout << "SUCCESS in createListenSocket\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "SUCCESS in createListenSocket\n");
        mServerStage = serverConnectStage::ServerSocketCreated;
    }
    
#ifdef windows_OS
    u_long iMode = 1;
    ioctlsocket(mListenSockfd, FIONBIO, &iMode); //Make it a non-blocking socket.
#else
    int flags;
    flags = fcntl(mListenSockfd, F_GETFL, 0);
    if (flags != -1)
        fcntl(mListenSockfd, F_SETFL, flags | O_NONBLOCK);
                
    bool bOptVal = true;
    if (setsockopt(mListenSockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&bOptVal, sizeof(BOOL)) == -1)
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - FAILED to set socket options!!!!!");

#endif
}

void dataSource::bindListenSocket()
{
    struct sockaddr_in source_addr;

#ifdef windows_OS
    ZeroMemory((char*)&source_addr, sizeof(source_addr));
#else
    bzero((char*)&source_addr, sizeof(source_addr));
#endif

    source_addr.sin_family      = AF_INET;
    source_addr.sin_addr.s_addr = inet_addr(mSourceIP);
    source_addr.sin_port        = htons(mPort);

    if (bind(mListenSockfd, (struct sockaddr*)&source_addr, sizeof(source_addr)) < 0) {
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - ERROR in bindListenSocket!!!!!");
        if (mDebugToConsole)
            std::cout << "ERROR in bindListenSocket. Error: " << errno << " " << strerror(errno) << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "ERROR in bindListenSocket. Error: %d  %s\n\n", errno, strerror(errno));
    } else {
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - SUCCESS in bindListenSocket!!!!!");
        if (mDebugToConsole)
            std::cout << "SUCCESS in bindListenSocket " << mListenSockfd << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "SUCCESS in bindListenSocket %d \n", mListenSockfd);
        mServerStage = ServerSocketBound;
    }
}

void dataSource::connectListenSocket()
{
    int n;
    n = listen(mListenSockfd, 10);
    if (n == -1) //SOCKET_ERROR)
    {
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - ERROR in connectListenSocket!!!!!");
        if (mDebugToConsole)
            std::cout << "ERROR in connectListenSocket!  errno " << errno << "  " << strerror(errno) << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "ERROR in connectListenSocket. Error: %d  %s\n\n", errno, strerror(errno));
    } else {
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - SUCCESS in connectListenSocket!!!!!");
        if (mDebugToConsole)
            std::cout << "SUCCESS in connectListenSocket\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "SUCCESS in connectListenSocket \n");

        allocateBuffers();
        mServerStage = ServerSocketListening;
    }
}

void dataSource::listenForConnection()
{
    mPacketCount = 0;
    mWorkSockfd = accept(mListenSockfd, NULL, NULL);
    if (mWorkSockfd == -1) {
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - ERROR in listenForConnection!!!!!");
        if (mDebugToConsole && (errno != 11)) //11 = resource unavailable, ie waiting for connection.
            std::cout << "ERROR in listenForConnection. Error " << errno << "   " << strerror(errno) << "\n";
        if (mDebugToFile && (errno != 11))
            fprintf(mDebugLog, "ERROR in listenForConnection. Error: %d  %s\n\n", errno, strerror(errno));
    } else {
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - SUCCESS in listenForConnection!!!!!");
        if (mDebugToConsole)
            std::cout << "SUCCESS in listenForConnection.  workSock: " << mWorkSockfd << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "SUCCESS in listenForConnection. workSock: %d \n\n", mWorkSockfd);
        mServerStage = ServerSocketAccepted;
    }
}

void dataSource::receivePacket()
{
    int n = recv(mWorkSockfd, mReturnBuffer, mPacketSize, 0); //mWorkSockfd
    if (n < 0) {
        char errorMessage[1024];
        sprintf(errorMessage, "dataSource - ERROR in receivePacket %d  %s", errno, strerror(errno));
        SG_LOG(SG_GENERAL, SG_INFO, errorMessage);
        if (mDebugToConsole)
            std::cout << "ERROR in receivePacket.  Error: " << errno << "  " << strerror(errno) << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "ERROR in receivePacket. Error: %d  %s\n\n", errno, strerror(errno));
    } else {
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource - SUCCESS in receivePacket!!!!!");
        if (mDebugToConsole)
            std::cout << "SUCCESS in receivePacket. Size = : " << n << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "SUCCESS in receivePacket. Size: %d \n\n", n);
        readPacket();
        //mServerStage = PacketReceived;
    }
}

void dataSource::readPacket()
{
    short opcode, controlCount; //,packetCount;
    controlCount = readShort();
    for (short i = 0; i < controlCount; i++) {
        opcode = readShort();

        char message[1024];
        sprintf(message, "Reading packet, opcode = %d", opcode);
        SG_LOG(SG_GENERAL, SG_INFO, message);
        if (mDebugToConsole)
            std::cout << "Reading packet, opcode = " << opcode << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "Reading packet, opcode = %d \n\n", opcode);

        if (opcode == OPCODE_BASE) 
        {
            handleBaseRequest();
        }
        else if (opcode == OPCODE_QUIT)
        {
            handleQuitRequest();
        }
        else if (opcode == OPCODE_TEST) 
        {
            handleTestRequest();            
        }
    }
    clearReturnPacket();
    //mServerStage = PacketRead;
    mServerStage = ServerSocketAccepted;
}

void dataSource::allocateBuffers()
{
    if (!mReturnBuffer) {
        mReturnBuffer = new char[mPacketSize];
        memset((void*)(mReturnBuffer), 0, mPacketSize);
    }
    if (!mSendBuffer) {
        mSendBuffer = new char[mPacketSize];
        memset((void*)(mSendBuffer), 0, mPacketSize);
    }
    if (!mStringBuffer) {
        mStringBuffer = new char[mPacketSize];
        memset((void*)(mStringBuffer), 0, mPacketSize);
    }
}

/////////////////////////////////////////////////////////////////////////////////

void dataSource::connectSendSocket()
{
    struct sockaddr_in source_addr;

    mReturnBuffer = new char[mPacketSize];
    mSendBuffer   = new char[mPacketSize];
    mStringBuffer = new char[mPacketSize];
    memset((void*)(mReturnBuffer), 0, mPacketSize);
    memset((void*)(mSendBuffer), 0, mPacketSize);
    memset((void*)(mStringBuffer), 0, mPacketSize);
    
    mWorkSockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mWorkSockfd < 0) {
        printf("ERROR opening send socket\n");
        return;
    }

    mReadyForRequests = true;

//#ifdef windows_OS //Whoops! I guess not! This breaks the hell out of everything.
    //u_long iMode = 1;
    //ioctlsocket(mWorkSockfd, FIONBIO, &iMode); //Make it a non-blocking socket.
//#endif

#ifdef windows_OS
    ZeroMemory((char*)&source_addr, sizeof(source_addr));
#else
    bzero((char*)&source_addr, sizeof(source_addr));
#endif

    source_addr.sin_family      = AF_INET;
    source_addr.sin_addr.s_addr = inet_addr(mSourceIP);
    source_addr.sin_port        = htons(mPort);
    int connectCode = connect(mWorkSockfd, (struct sockaddr*)&source_addr, sizeof(source_addr));
    if (connectCode < 0) 
    {
        printf("dataSource: ERROR connecting send socket: %d  IP: %s   Port: %d\n", connectCode, mSourceIP, mPort);
        return;
    } 
    else 
    {
        printf("dataSource: SUCCESS connecting send socket: %d\n", connectCode);
        mClientStage = ClientSocketConnected;
    }
}

void dataSource::sendPacket()
{
    memset((void*)(mStringBuffer), 0, mPacketSize);
    memcpy((void*)mStringBuffer, reinterpret_cast<void*>(&mSendControls), sizeof(short));
    memcpy((void*)&mStringBuffer[sizeof(short)], (void*)mSendBuffer, mPacketSize - sizeof(short));
    int n = send(mWorkSockfd, mStringBuffer, mPacketSize, 0);
    if (n < 0)
        printf("dataSource: ERROR sending  packet: %d\n", n);
    mLastSendTick = mCurrentTick;
    clearSendPacket();

    mClientStage = ClientSocketConnected;
}

void dataSource::clearSendPacket()
{
    memset((void*)(mSendBuffer), 0, mPacketSize);
    memset((void*)(mStringBuffer), 0, mPacketSize);

    mSendControls    = 0;
    mSendByteCounter = 0;
}

void dataSource::clearReturnPacket()
{
    memset((void*)(mReturnBuffer), 0, mPacketSize);
    memset((void*)(mStringBuffer), 0, mPacketSize);

    mReturnByteCounter = 0;
}
/////////////////////////////////////////////////////////////////////////////////

void dataSource::disconnectSockets()
{
#ifdef windows_OS
    closesocket(mListenSockfd);
    closesocket(mWorkSockfd);
#else
    close(mListenSockfd);
    close(mWorkSockfd);
#endif
}

/////////////////////////////////////////////////////////////////////////////////

void dataSource::writeShort(short value)
{
    memcpy((void*)&mSendBuffer[mSendByteCounter], reinterpret_cast<void*>(&value), sizeof(short));
    mSendByteCounter += sizeof(short);
}

void dataSource::writeInt(int value)
{
    memcpy((void*)&mSendBuffer[mSendByteCounter], reinterpret_cast<void*>(&value), sizeof(int));
    mSendByteCounter += sizeof(int);
}

void dataSource::writeFloat(float value)
{
    memcpy((void*)&mSendBuffer[mSendByteCounter], reinterpret_cast<void*>(&value), sizeof(float));
    mSendByteCounter += sizeof(float);
}

void dataSource::writeDouble(double value)
{
    memcpy((void*)&mSendBuffer[mSendByteCounter], reinterpret_cast<void*>(&value), sizeof(double));
    mSendByteCounter += sizeof(double);
}

void dataSource::writeString(const char* content)
{
    int length = strlen(content);
    writeInt(length);
    strncpy(&mSendBuffer[mSendByteCounter], content, length);
    mSendByteCounter += length;
}

//void dataSource::writePointer(void *pointer) //Maybe, someday? using boost, shared pointer or shared memory?
//{  //Or can it be done in a more brute force way with global scale pointers?
//}

//////////////////////////////////////////////

short dataSource::readShort()
{
    char bytes[sizeof(short)];
    for (int i = 0; i < sizeof(short); i++) bytes[i] = mReturnBuffer[mReturnByteCounter + i];
    mReturnByteCounter += sizeof(short);
    short* ptr = reinterpret_cast<short*>(bytes);
    return *ptr;
}

int dataSource::readInt()
{
    char bytes[sizeof(int)];
    for (int i = 0; i < sizeof(int); i++) bytes[i] = mReturnBuffer[mReturnByteCounter + i];
    mReturnByteCounter += sizeof(int);
    int* ptr = reinterpret_cast<int*>(bytes);
    return *ptr;
}

float dataSource::readFloat()
{
    char bytes[sizeof(float)];
    for (int i = 0; i < sizeof(float); i++) bytes[i] = mReturnBuffer[mReturnByteCounter + i];
    mReturnByteCounter += sizeof(float);
    float* ptr = reinterpret_cast<float*>(bytes);
    return *ptr;
}

double dataSource::readDouble()
{
    char bytes[sizeof(double)];
    for (int i = 0; i < sizeof(double); i++) bytes[i] = mReturnBuffer[mReturnByteCounter + i];
    mReturnByteCounter += sizeof(double);
    double* ptr = reinterpret_cast<double*>(bytes);
    return *ptr;
}

char* dataSource::readString()
{
    int length = readInt();
    strncpy(mStringBuffer, &mReturnBuffer[mReturnByteCounter], length);
    mReturnByteCounter += length;
    return mStringBuffer;
}

void dataSource::clearString()
{
    if (mStringBuffer != NULL)
        memset((void*)(mStringBuffer), 0, mPacketSize);
}

//void dataSource::readPointer()
//{
//}

/////////////////////////////////////////////////////////////////////////////////

void dataSource::addQuitRequest()
{
    short opcode = OPCODE_QUIT;
    mSendControls++;
    writeShort(opcode);
    writeInt(mCurrentTick);
}

void dataSource::handleQuitRequest()
{
    //fgShutdownHome();//Nope, including fg_init breaks compile with bind conflict.

    SG_LOG(SG_INPUT, SG_INFO, "dataSource: Program exit requested.");

    exitProgramTemp();//TEMP!

    //Damn! Every inclusion causes the bind conflict. All will be fixed if I ditch and go to telnet.
    //But meanwhile, I should be able to call out to nasal or something, to call a command.
    //fgSetBool("/sim/signals/exit", true);
    //globals->saveUserSettings();
    //fgOSExit(0);

    if (mDebugToConsole)
        std::cout << "handleQuitRequest \n";
    if (mDebugToFile)
        fprintf(mDebugLog, "handleQuitRequest\n\n");
}

void dataSource::addBaseRequest()
{
    short opcode = OPCODE_BASE;
    mSendControls++;
    writeShort(opcode);
    writeInt(mCurrentTick);
}

void dataSource::handleBaseRequest()
{
    int     tick = readInt();
    
    char message[1024];
    sprintf(message, "dataSource - handleBaseRequest - tick = %d", tick);
    SG_LOG(SG_GENERAL, SG_INFO, message);
    if (mDebugToConsole)
        std::cout << "dataSource clientTick = " << tick << ", my tick " << mCurrentTick;
    if (mDebugToFile)
        fprintf(mDebugLog, "dataSource clientTick %d, my tick %d\n\n", tick, mCurrentTick);
}

void dataSource::addTestRequest()
{
    short opcode = OPCODE_TEST;
    mSendControls++;
    writeShort(opcode);
    writeInt(mCurrentTick * 2);
    writeFloat((float)mCurrentTick * 0.999f);
    writeDouble((float)mCurrentTick * 10000000.0f);
    writeString("This is a test packet.");
}

void dataSource::handleTestRequest()
{
    int    tick = readInt();
    float  f    = readFloat();
    double d    = readDouble();
    char   myString[255];
    sprintf(myString, readString());

    
    char message[1024];
    sprintf(message, "dataSource - handleTestRequest - tick*2 =%d, float = %f, double = %f,  string = '%s'", tick, f, d, myString);
    SG_LOG(SG_GENERAL, SG_INFO, message);

    if (mDebugToConsole)
        std::cout << "handleTestRequest float = " << f << ", double = " << d << ", string ='" << myString << "'\n";
    if (mDebugToFile)
        fprintf(mDebugLog, "handleTestRequest float = %f, double = %f,  string = '%s'\n\n", f, d, myString);
}