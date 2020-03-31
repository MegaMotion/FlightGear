//-----------------------------------------------------------------------------
// Copyright (c) 2020 Chris Calef
//-----------------------------------------------------------------------------

#ifndef _CONTROLDATASOURCE_H_
#define _CONTROLDATASOURCE_H_

/////// Comment this out for linux builds.   //////////////
#define windows_OS 
///////////////////////////////////////////////////////////

#include "dataSource.h"

#define OPCODE_QUIT 10
#define OPCODE_ENGINE_START 11
#define OPCODE_RESET 12

/// Base class for various kinds of data sources, first one being worldDataSource, for terrain, sky, weather and map information.
class controlDataSource : public dataSource 
{
   public:
	   
	   controlDataSource(bool listening, bool alternating, int port, char *IP);
	   ~controlDataSource();

	   void tick();
       void readPacket();

	   void addQuitRequest();
	   void handleQuitRequest();

	   void addEngineStartRequest();
       void handleEngineStartRequest();

       void addResetRequest();
       void handleResetRequest();
};

#endif // _CONTROLDATASOURCE_H_
