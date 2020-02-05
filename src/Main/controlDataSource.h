//-----------------------------------------------------------------------------
// Copyright (c) 2015 Chris Calef
//-----------------------------------------------------------------------------

#ifndef _CONTROLDATASOURCE_H_
#define _CONTROLDATASOURCE_H_

/////// Comment this out for linux builds.   //////////////
#define windows_OS 
///////////////////////////////////////////////////////////

#include "dataSource.h"



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

};

#endif // _CONTROLDATASOURCE_H_
