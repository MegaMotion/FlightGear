//-----------------------------------------------------------------------------
// Copyright (c) 2020 Chris Calef
//-----------------------------------------------------------------------------

#ifndef _WORLDDATASOURCE_H_
#define _WORLDDATASOURCE_H_

/////// Comment this out for linux builds.   //////////////
#define windows_OS 
///////////////////////////////////////////////////////////

#include "dataSource.h"


#define OPCODE_INIT_TERRAIN 21
#define OPCODE_TERRAIN 22
#define OPCODE_INIT_SKYBOX 23
#define OPCODE_SKYBOX 24
#define OPCODE_TERRAIN_RESPONSE 25

/// Base class for various kinds of data sources, first one being worldDataSource, for terrain, sky, weather and map information.
class worldDataSource : public dataSource 
{
   public:
	   
       float    mTileWidth;
       int      mHeightMapRes;
       int      mTextureRes;
       float    mMapCenterLat;
       float    mMapCenterLong;

       float mMetersPerDegreeLatitude;
       float mMetersPerDegreeLongitude;
       float mDegreesPerMeterLatitude;
       float mDegreesPerMeterLongitude;
       
       float    mTileWidthLong;
       float    mTileWidthLat;



       std::string mResourcePath;
       
       unsigned int mSkyboxStage;
           
	   worldDataSource(bool listening, bool alternating, int port, char* IP);
       ~worldDataSource();

	   void tick();
       
       std::string getTileName(double tileStartPointLong, double tileStartPointLat);

       //void listenForPacket();
       void readPacket();

       //void addInitTerrainRequest(terrainPagerData* data, const char* path);
       void handleInitTerrainRequest();

       //void addTerrainRequest(float playerLong, float playerLat);
       void handleTerrainRequest();

       void addTerrainResponse(const char* tilename);
       //void handleTerrainResponse();

       //void addInitSkyboxRequest(unsigned int skyboxRes, int cacheMode, const char* path);
       //void handleInitSkyboxRequest();

       //void addSkyboxRequest(float tileLong, float tileLat, float playerLong, float playerLat, float playerAlt);
       //void handleSkyboxRequest();



};

#endif // _WORLDDATASOURCE_H_
