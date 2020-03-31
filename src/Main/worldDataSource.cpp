//-----------------------------------------------------------------------------
// Copyright (c) 2020 Chris Calef
//-----------------------------------------------------------------------------

#include "worldDataSource.h"
#include <simgear\debug\logstream.hxx>

extern void makeTerrainTile(worldDataSource* worldData, float startLong, float startLat, const char *tilename); //FIX: make this a data struct or class pointer, not a long string of args. Like maybe *this as a worldDataSource.

worldDataSource::worldDataSource(bool listening, bool alternating, int port, char* IP) : dataSource(listening, alternating, port, IP)
{
}

worldDataSource::~worldDataSource()
{
}

////////////////////////////////////////////////////////////////////////////////
void worldDataSource::tick()
{
    //char message[1024];
    if (mLastSendTick == 0) { //Okay, first let's make sure both sockets are connected and waiting, before we do any other work.
        if (mClientStage != ClientSocketConnected)
            connectSendSocket();
        if (mServerStage != ServerSocketListening)
            connectListenSocket();
        mLastSendTick = 1;//just to make sure we don't do this again.
    }

    if (mSendControls > 0) 
	{
       if (mClientStage == ClientSocketConnected)
            sendPacket();
        else
            connectSendSocket();
    } 
	else if (mListening) 
	{
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
            addTestRequest();
            sendPacket();
            break;
        }
    }
    mCurrentTick++;
	*/


void worldDataSource::readPacket()
{
    short   opcode, controlCount; //,packetCount;
    char    message[1024];
    FD_SET  ReadSet;
    DWORD   total;
    timeval tval;
    tval.tv_sec  = 0;
    tval.tv_usec = 30;

    FD_ZERO(&ReadSet);
    FD_SET(mWorkSockfd, &ReadSet);

    //THIS is how you actually enforce non-blocking status! Recv will just wait, on its own.
    total = select(0, &ReadSet, NULL, NULL, &tval);
    if (total == 0)
        return;

    int n = recv(mWorkSockfd, mReturnBuffer, mPacketSize, 0); //mWorkSockfd
    if (n < 0)
        return;

    controlCount = readShort();
    for (short i = 0; i < controlCount; i++) {
        opcode = readShort();

        sprintf(message, "worldDataSource Reading packet, opcode = %d", opcode);
        SG_LOG(SG_GENERAL, SG_INFO, message);
        if (mDebugToConsole)
            std::cout << "Reading packet, opcode = " << opcode << "\n";
        if (mDebugToFile)
            fprintf(mDebugLog, "Reading packet, opcode = %d \n\n", opcode);

        if (opcode == OPCODE_BASE) {
            handleBaseRequest();
        } else if (opcode == OPCODE_TEST) {
            handleTestRequest();
        } else if (opcode == OPCODE_INIT_TERRAIN) {
            handleInitTerrainRequest();
        } else if (opcode == OPCODE_TERRAIN) {
            handleTerrainRequest();
        }
    }
    clearReturnPacket();
    //mServerStage = PacketRead;
    //mServerStage = ServerSocketAccepted;
    //if (mAlternating)
    //    mListening = false;
}


void worldDataSource::handleInitTerrainRequest()
{
    SG_LOG(SG_GENERAL, SG_INFO, "HANDLING INIT TERRAIN REQUEST!!!");

    //WARNING: we should probably verify valid values here before assigning them, but if we're sure...?
    mTileWidth     = readFloat();
    mHeightMapRes  = readInt();
    mTextureRes    = readInt();
    mMapCenterLong = readFloat();
    mMapCenterLat  = readFloat();
    mResourcePath  = readString();

    float rLat = (mMapCenterLat * M_PI) / 180.0f;

    mMetersPerDegreeLatitude  = 111132.92 - 559.82 * cos(2 * rLat) + 1.175 * cos(4 * rLat);
    mMetersPerDegreeLongitude = 111412.84 * cos(rLat) - 93.5 * cos(3 * rLat);
    mDegreesPerMeterLatitude  = 1.0f / mMetersPerDegreeLatitude;
    mDegreesPerMeterLongitude = 1.0f / mMetersPerDegreeLongitude;

    mTileWidthLong = mDegreesPerMeterLongitude * mTileWidth;
    mTileWidthLat  = mDegreesPerMeterLatitude * mTileWidth;

    char message[1024];
    sprintf(message, "dataSource - handleInitTerrainRequest - tileWidth %f  heightMapRes %d textureRes %d mapCenterLong %f lat %f resourcePath %s tileWidthLong %f Lat %f",
            mTileWidth, mHeightMapRes, mTextureRes, mMapCenterLong, mMapCenterLat, mResourcePath.c_str(), mTileWidthLong, mTileWidthLat);
    SG_LOG(SG_GENERAL, SG_INFO, message);
}

void worldDataSource::handleTerrainRequest()
{
    float startLong = readFloat();
    float startLat  = readFloat();

    char message[1024];
    sprintf(message, "dataSource - handleTerrainRequest - Long %f Lat %f", startLong, startLat);
    SG_LOG(SG_GENERAL, SG_INFO, message);


	std::string tileName = getTileName((double)startLong, (double)startLat);

    makeTerrainTile(this, startLong, startLat, tileName.c_str());

	/* // Just testing... yes, as expected, this all works fine. So why does textureBuffer crash me cold?
	char* buffer;
    buffer    = (char*)malloc(1048576);
    buffer[0] = 'a';
    buffer[1] = 'B';
    buffer[2] = 'o';
    buffer[3] = 'u';
    buffer[4] = 'T';
    buffer[5] = (char)44;
    buffer[6] = (char)45;
    buffer[7] = (char)46;
    buffer[8] = '\0';
    SG_LOG(SG_GENERAL, SG_INFO, buffer);
	free(buffer);
	*/
    
	addTerrainResponse(tileName.c_str());
}


void worldDataSource::addTerrainResponse(const char* tilename)
{
    short opcode = OPCODE_TERRAIN_RESPONSE;	
    mSendControls++;
    writeShort(opcode);
    writeInt(mCurrentTick);
    writeString(tilename);

    char message[1024];
    sprintf(message, "worldDataSource - addTerrainResponse - tilename %s opcode %d sendBytes %d", tilename, opcode, mSendByteCounter);
    SG_LOG(SG_GENERAL, SG_INFO, message);
}

//void worldDataSource::handleTerrainResponse() {}

std::string worldDataSource::getTileName(double tileStartPointLong, double tileStartPointLat)
{
    std::string returnStr;
    char        temp[255];

    double tileStartPointLongR, tileStartPointLatR;
    tileStartPointLongR = (float)((int)(tileStartPointLong * 1000.0)) / 1000.0; //Maybe?
    tileStartPointLatR  = (float)((int)(tileStartPointLat * 1000.0)) / 1000.0;  //Maybe?
    //Debug.Log("Pre rounding longitude: " + tileStartPointLong + ", post rounding: " + tileStartPointLongR +
    //			", pre latitude " + tileStartPointLat + " post " + tileStartPointLatR);
    char longC, latC;
    if (mMapCenterLong < 0)
        longC = 'W';
    else
        longC = 'E';
    if (mMapCenterLat < 0)
        latC = 'S';
    else
        latC = 'N';
    //NOW, just have to separate out the decimal part from the whole numbers part, and make sure to get
    //preceding zeroes for the decimal part, so it's always three characters.
    //NOTE: there is a Convert class in  C#, with various ToString functions, but this would all be C# specific,
    //might as well do it myself the hard way and it will at least be easy to convert it over to C++ as well.
    //string testString = Convert.ToString(tileStartPointLongR,??);
    //string longitudeName =
    int         majorLong, minorLong, majorLat, minorLat; //"major" for left of decimal, "minor" for right of decimal.
    std::string majorLongStr, minorLongStr, majorLatStr, minorLatStr;

    majorLong = (int)tileStartPointLongR;
    majorLat  = (int)tileStartPointLatR;

    //minorLong = Math.Abs((int)((tileStartPointLongR - (float)majorLong) * 1000.0f));//NOPE!  float errors creeping in...
    minorLong = abs((int)(tileStartPointLongR * 1000.0) - (majorLong * 1000)); //Turns out doubles fixed it.
    //minorLat = Math.Abs((int)((tileStartPointLatR - (float)majorLat) * 1000.0));
    minorLat = abs((int)(tileStartPointLatR * 1000.0) - (majorLat * 1000));

    majorLong = abs(majorLong);
    majorLat  = abs(majorLat);

    if (majorLong < 10)
        sprintf(temp, "00%d", majorLong);
    else if (majorLong < 100)
        sprintf(temp, "0%d", majorLong);
    else
        sprintf(temp, "%d", majorLong);
    majorLongStr = temp;

    if (majorLat < 10) //Latitude ends at 90 so no worries about three digits.
        sprintf(temp, "0%d", majorLat);
    else
        sprintf(temp, "%d", majorLat);
    majorLatStr = temp;

    if (minorLong < 10)
        sprintf(temp, "00%d", minorLong);
    else if (minorLong < 100)
        sprintf(temp, "0%d", minorLong);
    else
        sprintf(temp, "%d", minorLong);
    minorLongStr = temp;

    if (minorLat < 10)
        sprintf(temp, "00%d", minorLat);
    else if (minorLat < 100)
        sprintf(temp, "0%d", minorLat);
    else
        sprintf(temp, "%d", minorLat);
    minorLatStr = temp;

    //returnStr = majorLongStr + "d" + minorLongStr + longC + "_" + majorLatStr + "d" + minorLatStr + latC;
    sprintf(temp, "%sd%s%c_%sd%s%c", majorLongStr.c_str(), minorLongStr.c_str(), longC, majorLatStr.c_str(), minorLatStr.c_str(), latC);
    returnStr = temp;

    return returnStr;
}


/*
	
		double elevation_m = 0;
		SGGeod kPos;
		const simgear::BVHMaterial* bvhMat;
		const SGMaterial *mat;

		float playerPos_x = (player_long - mapCenterLong) * metersPerDegreeLong;
		float playerPos_z = (player_lat - mapCenterLat) * metersPerDegreeLat;
		float startPos_x,startPos_z;
		int playerGridX,playerGridZ;
	



		// NEW WAY.  Instead of ever basing startPos on player pos, make it even tile segments based on 
		// origin at mapCenter.

		
		startPos_x = (start_long - mapCenterLong) * metersPerDegreeLong;
		startPos_z = (start_lat - mapCenterLat) * metersPerDegreeLat;	
		
		playerGridX = (int)((playerPos_x - startPos_x) / tileWidth);
		playerGridZ = (int)((playerPos_z - startPos_z) / tileWidth);

		if ((playerGridX!=1)||(playerGridZ!=1))//We are not in the center tile anymore, gotta reload.
		{
			terrainLockfilePath = resourcePath + "lockfile.terrain.tmp";
			fwp = fopen(terrainLockfilePath.c_str(),"w");
			fclose(fwp);

			if (playerGridX==0)
				base_long -= tileWidthLong;
			else if (playerGridX==2)
				base_long += tileWidthLong;
			if  (playerGridZ==0)
				base_lat -= tileWidthLat;
			else if (playerGridZ==2)
				base_lat += tileWidthLat;

			if ((playerGridX==0)&&(playerGridZ==0)) {//lower left
				mLoadTerrains[2][0] = true;				
				mLoadTerrains[1][0] = true;					
				mLoadTerrains[0][0] = true;					
				mLoadTerrains[0][1] = true;					
				mLoadTerrains[0][2] = true;					
			} else if ((playerGridX==0)&&(playerGridZ==1)) {//mid left
				mLoadTerrains[2][0] = true;					
				mLoadTerrains[1][0] = true;						
				mLoadTerrains[0][0] = true;					
			} else if ((playerGridX==0)&&(playerGridZ==2)) {//upper left	
				mLoadTerrains[0][0] = true;					
				mLoadTerrains[1][0] = true;					
				mLoadTerrains[2][0] = true;					
				mLoadTerrains[2][1] = true;					
				mLoadTerrains[2][2] = true;					
			} else if ((playerGridX==1)&&(playerGridZ==2)) {//upper middle
				mLoadTerrains[2][0] = true;							
				mLoadTerrains[2][1] = true;					
				mLoadTerrains[2][2] = true;						
			} else if ((playerGridX==2)&&(playerGridZ==2)) {//upper right	
				mLoadTerrains[0][2] = true;
				mLoadTerrains[1][2] = true;
				mLoadTerrains[2][2] = true;
				mLoadTerrains[2][1] = true;
				mLoadTerrains[2][0] = true;					
			} else if ((playerGridX==2)&&(playerGridZ==1)) {//mid right
				mLoadTerrains[2][2] = true;
				mLoadTerrains[1][2] = true;						
				mLoadTerrains[0][2] = true;				
			} else if ((playerGridX==2)&&(playerGridZ==0)) {//lower right
				mLoadTerrains[2][2] = true;						
				mLoadTerrains[1][2] = true;					
				mLoadTerrains[0][2] = true;	
				mLoadTerrains[0][1] = true;					
				mLoadTerrains[0][0] = true;						
			} else if ((playerGridX==1)&&(playerGridZ==0)) {//lower middle
				mLoadTerrains[0][0] = true;
				mLoadTerrains[0][1] = true;	
				mLoadTerrains[0][2] = true;			
			} 

			startPos_x = (start_long - mapCenterLong) * metersPerDegreeLong;
			startPos_z = (start_lat - mapCenterLat) * metersPerDegreeLat;


			float heightSizeLong = tileWidthLong / (float)(heightmapRes-1);
			float heightSizeLat = tileWidthLat / (float)(heightmapRes-1);

			float textelSizeLong = tileWidthLong / (float)textureRes;
			float textelSizeLat = tileWidthLat / (float)textureRes;
			float textelSizeMeters = tileWidth / (float)textureRes;//redundant?


			//Now, if we have an whole multiple here we can grab height values as we go through the textures, 
			//otherwise they'll need their own loop.
			float texels_per_height_flt = (float)textureRes/(float)(heightmapRes-1);
			int texels_per_height = (int)texels_per_height_flt;
			if ((float)texels_per_height!=texels_per_height_flt)
				texels_per_height = 0;//If it's zero then it didn't work.

			int totalTextureSize = textureRes;
			int totalHeightmapSize = heightmapRes;



			const int texture_array_size = totalTextureSize * totalTextureSize;
			int array_pos = 0;
			int bytes_remaining = texture_array_size;
			int linenum = 0;
			char *textureBuffer;
			textureBuffer = new char[texture_array_size];

			int num_height_bin_args = 5;// lat, long, tileWidth, heightmapRes, textureRes
			const int height_array_size = (totalHeightmapSize * totalHeightmapSize) + num_height_bin_args;
			float *heightBuffer;
			char *heightCharBuffer;
			heightBuffer = new float[height_array_size];
			char tempChars[255];
			FGScenery *kScenery = globals->get_scenery();
			for (int x=0;x<3;x++) 
			{
				for (int y=0;y<3;y++) 
				{
					start_long = base_long + (x * tileWidthLong);
					start_lat = base_lat + (y * tileWidthLat);
					
					bool textExists = false;
					sprintf(tempChars,"text.%s.bin",getTileName(start_long,start_lat).c_str());
					std::string textureBinfilePath = resourcePath + tempChars;
					fp = fopen(textureBinfilePath.c_str(),"r");
					if (fp) {
						textExists = true;
						printf("Texture file exists!  %s\n",textureBinfilePath.c_str());
						fclose(fp);
					}

					bool hghtExists = false;	
					sprintf(tempChars,"hght.%s.bin",getTileName(start_long,start_lat).c_str());
					std::string heightBinfilePath = resourcePath + tempChars;
					fp = fopen(heightBinfilePath.c_str(),"r");
					if (fp) {
						hghtExists = true;
						printf("Height file exists!  %s\n",heightBinfilePath.c_str());
						fclose(fp);
					}
					if ((mLoadTerrains[y][x])&&(!textExists)&&(!hghtExists))
					{				
						printf("trying to load terrain: %d %d",y,x);
						kPos.setLatitudeDeg(start_lat );
						kPos.setLongitudeDeg(start_long );
						kPos.setElevationM(0);

						heightBuffer[0] = start_long;
						heightBuffer[1] = start_lat;
						heightBuffer[2] = tileWidth;
						heightBuffer[3] = (float)heightmapRes;
						heightBuffer[4] = (float)textureRes;

						vector<string> unidentified_textures;
						
						//HERE:  Now we have to decide whether to load based on mLoadTerrains[][], instead of loading whole area.
						for (int i=0;i<totalTextureSize;i++)
						{
							for (int j=0;j<totalTextureSize;j++)
							{
								elevation_m = 0.0f;
								kPos.setLatitudeDeg(start_lat + (j*textelSizeLat));
								kPos.setLongitudeDeg(start_long + (i*textelSizeLong));
								kPos.setElevationM(0);
								//printf("looking for elevation %d %d: %f %f\n",j,i,start_lat + (j*textelSizeLat),start_long + (i*textelSizeLong));
								if (kScenery->get_elevation_m(SGGeod::fromGeodM(kPos, 3000),
									elevation_m, &bvhMat, 0))
								{
									mat = dynamic_cast<const SGMaterial *>(bvhMat);
									if (false)//((texels_per_height)&&(i%texels_per_height==0)&&(j%texels_per_height==0))
									{//Here:  if we are right on a line, then save the height data.
										heightBuffer[((j/texels_per_height)*totalHeightmapSize)+(i/texels_per_height)+num_height_bin_args] = elevation_m;
									}
									if (mat) 
									{
										if (!strcmp(mat->get_names()[0].c_str(),"BuiltUpCover"))
											textureBuffer[(j*totalTextureSize)+i]=(char)1;
										else if (!strcmp(mat->get_names()[0].c_str(),"EvergreenBroadCover"))
											textureBuffer[(j*totalTextureSize)+i]=(char)2;
										else if (!strcmp(mat->get_names()[0].c_str(),"DryCropPastureCover"))
											textureBuffer[(j*totalTextureSize)+i]=(char)3;
										else if (!strcmp(mat->get_names()[0].c_str(),"Grass"))
											textureBuffer[(j*totalTextureSize)+i]=(char)4;
										else if (!strcmp(mat->get_names()[0].c_str(),"Sand"))
											textureBuffer[(j*totalTextureSize)+i]=(char)5;
										else if ((!strcmp(mat->get_names()[0].c_str(),"Lake"))||
											(!strcmp(mat->get_names()[0].c_str(),"Pond"))||
											(!strcmp(mat->get_names()[0].c_str(),"Reservoir"))||
											(!strcmp(mat->get_names()[0].c_str(),"Stream"))||
											(!strcmp(mat->get_names()[0].c_str(),"Canal"))||
											(!strcmp(mat->get_names()[0].c_str(),"Lagoon"))||
											(!strcmp(mat->get_names()[0].c_str(),"Estuary"))||
											(!strcmp(mat->get_names()[0].c_str(),"Watercourse"))||
											(!strcmp(mat->get_names()[0].c_str(),"Saline")))
											textureBuffer[(j*totalTextureSize)+i]=(char)6;
										else 
										{
											bool newUnID = true;
											for (int k=0;k<unidentified_textures.size();k++)
											{
												if (!strcmp(unidentified_textures[k].c_str(),mat->get_names()[0].c_str()))
													newUnID = false;
											}
											if (newUnID)
											{
												unidentified_textures.insert(unidentified_textures.begin(),mat->get_names()[0]);
												printf("Unidentified texture: %s\n",mat->get_names()[0].c_str());
											}

											textureBuffer[(j*totalTextureSize)+i]=(char)0;
										}
									} else {
										textureBuffer[(j*totalTextureSize)+i]=(char)0;
									}
								}
							}
						}
						if (true)//(texels_per_height==0)
						{
							for (int i=0;i<totalHeightmapSize;i++)
							{
								for (int j=0;j<totalHeightmapSize;j++)
								{
									elevation_m = 0.0f;
									kPos.setLatitudeDeg(start_lat + (j*heightSizeLat));
									kPos.setLongitudeDeg(start_long + (i*heightSizeLong));
									kPos.setElevationM(0);
									if (kScenery->get_elevation_m(SGGeod::fromGeodM(kPos, 3000),
										elevation_m, &bvhMat, 0))
									{
										heightBuffer[(j*totalHeightmapSize)+i+num_height_bin_args] = elevation_m;
									}
								}
							}
						}
						//printf("FOUND ALL THE MATERIALS! %s  total size %d\n",mat->get_names()[0].c_str(),totalTextureSize*totalTextureSize);

						FILE *fp = NULL;
						//std::string textureBinfilePath = resourcePath + "terrain.textures.bin";
						//sprintf(tempChars,"terrain_%d_%d.textures.bin",y,x);
						printf("Writing to texture bin file: %s\n", textureBinfilePath.c_str());
						if (!(fp = fopen(textureBinfilePath.c_str(),"wb")))
							return;		
						fwrite(textureBuffer,texture_array_size,1,fp);
						fclose(fp);

						//std::string heightBinfilePath = resourcePath + "terrain.heights.bin";
						//sprintf(tempChars,"terrain_%d_%d.heights.bin",y,x);
						printf("Writing to heights bin file: %s  start lat/long %f  %f \n", 
							heightBinfilePath.c_str(),start_lat,start_long);
						if (!(fp = fopen(heightBinfilePath.c_str(),"wb")))
							return;		
						heightCharBuffer = reinterpret_cast<char*>(heightBuffer);
						fwrite(heightCharBuffer,height_array_size*sizeof(float),1,fp);
						fclose(fp);		
					}
				}
			}
			printf("made the files! texels_per_height %d  startPos %f,%f \n",
				texels_per_height,startPos_x,startPos_z);
			
			printf("removing terrain lockfile: %s\n",terrainLockfilePath.c_str());
			remove(terrainLockfilePath.c_str());


		}

	
  //TEMP, dataSource
  const simgear::BVHMaterial* bvhMat = intersectVisitor.getMaterial();
  int travnum = intersectVisitor.getTraversalNumber();
  osg::NodePath nPath = intersectVisitor.getNodePath();

  osg::NodeVisitor::VisitorType vType = intersectVisitor.getVisitorType();
  osg::Geode* geode = new osg::Geode;
  osg::Group* tGroup = globals->get_scenery()->get_terrain_branch();
  
  SGMaterialLib *matlib = globals->get_matlib();
  const SGMaterial *mat = dynamic_cast<const SGMaterial *>(bvhMat);
  if (mat)
  {
	  SGVec2f texScale = mat->get_tex_coord_scale();
	  for (int i=0;i<mat->get_names().size();i++)
	  {
		printf("mat names[%d] %s count %d  xsize %f  ysize %f\n",i,mat->get_names()[i].c_str(),
			SGReferenced::count(bvhMat),mat->get_xsize(),mat->get_ysize());
	  }
  }


*/
//float bytes_to_float(unsigned char HH, unsigned char HL, unsigned char LH, unsigned char LL)
//{
//	float value = 0;
//	char data[4];
//
//	data[0] = HL;
//	data[1] = HH;
//	data[2] = LL;
//	data[3] = LH;
//	//data[0] = LL;
//	//data[1] = LH;
//	//data[2] = HL;
//	//data[3] = HH;
//
//	float *fptr = (float *)data;
//	value = *fptr;
//	printf("bytes: %d %d %d %d,  float %f \n",LL,LH,HL,HH,value);
//	return value;
//}


/*
void skyboxSocketListen()
{    	
	//SG_LOG(SG_GENERAL, SG_INFO, "dataSource: skyboxSocket Listening!!!!!!!!!!!!!");
	//printf("skyboxSocketListen, stage %d \n",skyboxStage);
	printf("skyboxStage %d .. ",skyboxStage);
   
	if (skyboxStage==1)
	{//Here, need timing mechanism!  Do not call draw again until image is done.
		fgDumpSnapShot();
		skyboxStage=2;
		return;
	} 
	else if (skyboxStage==2)
	{
		skyboxSocketDraw();
		skyboxStage=0;
		return;
	}

	int clilen;
	const int packetsize = 1024;//Turns out this isn't necessary as an actual packet size, TCP handles that.
	char buffer[packetsize];//,buffer2[packetsize];//I do need a number for my input buffer size though.

	struct sockaddr_in cli_addr;
	int n;
	FILE *fwp,*fp;

	struct timeval tv;

	tv.tv_sec = sockTimeout;
	tv.tv_usec = 0;

	listen(sockfd,1);//5 is backlog number, might want more if we get busy.

	readfds = master;
	select(sockfd+1, &readfds, NULL, NULL, &tv);
    //SG_LOG(SG_GENERAL, SG_INFO, "dataSource: skyboxSocketabout to Listen!!!!!!!!!!!!!");
	if (FD_ISSET(sockfd, &readfds))
	{
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, 
			&clilen);
        if (newsockfd < 0) {
            SG_LOG(SG_GENERAL, SG_INFO, "dataSource: error on accept!!!!!!!!!!!");
			return;
        } else {
            SG_LOG(SG_GENERAL, SG_INFO, "dataSource: accepted socket connection!!!!!!!!!!!!!");
		}

		FD_SET(newsockfd,&master);

		ZeroMemory(buffer, sizeof(packetsize));
		n = recv(newsockfd,buffer,packetsize,0);
		//n = recvfrom(newsockfd,buffer,packetsize,0,cli_addr,sizeof(cli_addr));
		if (n < 0) 
		{
            SG_LOG(SG_GENERAL, SG_INFO, "dataSource: error reading from socket!!!!!!!!!!!!");
			return;
        }
        SG_LOG(SG_GENERAL, SG_INFO, "dataSource: Reading ___ bytes!!!!!!!!!!!!!");
				
		bool mLoadTerrains[3][3];
		int num_args = 11;

		char resourcePathArray[1024];
		char *bytes = &(buffer[0]);
		//vector<unsigned char> byteVec(bytes,bytes + sizeof(float) * num_args);
		//float *argArray = reinterpret_cast<float*>(bytes);
		float *argArray = reinterpret_cast<float*>(bytes);


		//printf("ARGhs:  %f %f %f %f %f %f %f %f %f %f %f\n",argArray[0],argArray[1],argArray[2],argArray[3],
		//		argArray[4],argArray[5],argArray[6],argArray[7],argArray[8],argArray[9],argArray[10]);
		float player_long = argArray[0];
		int i=0;
		//float player_long = bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//
		float player_lat = argArray[1];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//
		float player_elev = argArray[2];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//
		//HERE:  New changes coming!  Now we are going to start writing a large (5x5 terrain tile) area
		//at one time, to a set of bin files (one file for height data, one for textures, so far.)
		float tileWidth = argArray[3];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//Total distance covered in meters by one tile, assumed to be square.
		int heightmapRes = (int)argArray[4];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//
		int textureRes = (int)argArray[5];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//
		float tileWidthLong = argArray[6];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//
		float tileWidthLat = argArray[7];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//
		float base_long = argArray[8];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//
		float base_lat = argArray[9];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//
		mFullRebuild = (bool)argArray[10];//bytes_to_float(buffer[i+0],buffer[i+1],buffer[i+2],buffer[i+3]); i+=4;//

		float start_long = base_long;
		float start_lat = base_lat;

		sprintf(resourcePathArray,&buffer[sizeof(float) * num_args]);
		resourcePath = resourcePathArray;
		printf("player longitude:  %f  latitude  %f   elev  %f tileWidth %f startLong/Lat %f %f path %s\n",
			player_long,player_lat,player_elev,tileWidth,start_long,start_lat,resourcePath.c_str());

		float metersPerDegreeLong = tileWidth / tileWidthLong;
		float metersPerDegreeLat = tileWidth / tileWidthLat;
		float degreesPerMeterLong = tileWidthLong / tileWidth;
		float degreesPerMeterLat = tileWidthLat / tileWidth;

		double elevation_m = 0;
		SGGeod kPos;
		const simgear::BVHMaterial* bvhMat;
		const SGMaterial *mat;

		float playerPos_x = (player_long - mapCenterLong) * metersPerDegreeLong;
		float playerPos_z = (player_lat - mapCenterLat) * metersPerDegreeLat;
		float startPos_x,startPos_z;
		int playerGridX,playerGridZ;
				
		for (int i=0;i<3;i++)
			for (int j=0;j<3;j++)
				if (mFullRebuild)
					mLoadTerrains[j][i] = true;
				else
					mLoadTerrains[j][i] = false;

		//First, decide if we need new terrain, if we do then do that first and forget about skyboxes.
		//SO...  now we have startLong/startLat, which are the coordinates of the left bottom corner of the 
		//current tile set.  
		//HERE:  NEW WAY.  Instead of ever basing startPos on player pos, make it even tile segments based on 
		//origin at mapCenter.
		//if (mFullRebuild)
		//{//Here, ignore whatever startpos was sent, figure one out based on tileDistance * 1.5 from player.
		//	startPos_x = playerPos_x - (tileWidth * 1.5);
		//	startPos_z = playerPos_z - (tileWidth * 1.5);
		//	start_long = (startPos_x * degreesPerMeterLong) + mapCenterLong;
		//	start_lat  = (startPos_z * degreesPerMeterLat) + mapCenterLat;
		//	//printf("Full rebuild: startPos %f,%f  playerPos %f,%f startLong/Lat %f,%f\n",
		//	//	startPos_x,startPos_z,playerPos_x,playerPos_z,start_long,start_lat);
		//}  else {
		startPos_x = (start_long - mapCenterLong) * metersPerDegreeLong;
		startPos_z = (start_lat - mapCenterLat) * metersPerDegreeLat;				
		//}

		playerGridX = (int)((playerPos_x - startPos_x) / tileWidth);
		playerGridZ = (int)((playerPos_z - startPos_z) / tileWidth);
		printf("Player grid: [ %d , %d ]  playerPos %f,%f  startPos %f,%f  start_long/lat %f %f\n",
			playerGridX,playerGridZ,playerPos_x,playerPos_z,startPos_x,startPos_z,start_long,start_lat);

		if ((mFullRebuild)||(playerGridX!=1)||(playerGridZ!=1))//We are not in the center tile anymore, gotta reload.
		{//WARNING this method has no buffer zone, so will be vulnerable to frequent reload if we zigzag over the line.
			//HERE: one more quick check could determine if the relevant value is far enough over the line to 
			//be worth a reload.
			terrainLockfilePath = resourcePath + "lockfile.terrain.tmp";
			fwp = fopen(terrainLockfilePath.c_str(),"w");
			fclose(fwp);

			if (!mFullRebuild)
			{
				if (playerGridX==0)
					base_long -= tileWidthLong;
				else if (playerGridX==2)
					base_long += tileWidthLong;
				if  (playerGridZ==0)
					base_lat -= tileWidthLat;
				else if (playerGridZ==2)
					base_lat += tileWidthLat;

				//Now, repeat the same logic we did on unity side to determine who to reload:
				if ((playerGridX==0)&&(playerGridZ==0)) {//lower left
					mLoadTerrains[2][0] = true;				
					mLoadTerrains[1][0] = true;					
					mLoadTerrains[0][0] = true;					
					mLoadTerrains[0][1] = true;					
					mLoadTerrains[0][2] = true;					
				} else if ((playerGridX==0)&&(playerGridZ==1)) {//mid left
					mLoadTerrains[2][0] = true;					
					mLoadTerrains[1][0] = true;						
					mLoadTerrains[0][0] = true;					
				} else if ((playerGridX==0)&&(playerGridZ==2)) {//upper left	
					mLoadTerrains[0][0] = true;					
					mLoadTerrains[1][0] = true;					
					mLoadTerrains[2][0] = true;					
					mLoadTerrains[2][1] = true;					
					mLoadTerrains[2][2] = true;					
				} else if ((playerGridX==1)&&(playerGridZ==2)) {//upper middle
					mLoadTerrains[2][0] = true;							
					mLoadTerrains[2][1] = true;					
					mLoadTerrains[2][2] = true;						
				} else if ((playerGridX==2)&&(playerGridZ==2)) {//upper right	
					mLoadTerrains[0][2] = true;
					mLoadTerrains[1][2] = true;
					mLoadTerrains[2][2] = true;
					mLoadTerrains[2][1] = true;
					mLoadTerrains[2][0] = true;					
				} else if ((playerGridX==2)&&(playerGridZ==1)) {//mid right
					mLoadTerrains[2][2] = true;
					mLoadTerrains[1][2] = true;						
					mLoadTerrains[0][2] = true;				
				} else if ((playerGridX==2)&&(playerGridZ==0)) {//lower right
					mLoadTerrains[2][2] = true;						
					mLoadTerrains[1][2] = true;					
					mLoadTerrains[0][2] = true;	
					mLoadTerrains[0][1] = true;					
					mLoadTerrains[0][0] = true;						
				} else if ((playerGridX==1)&&(playerGridZ==0)) {//lower middle
					mLoadTerrains[0][0] = true;
					mLoadTerrains[0][1] = true;	
					mLoadTerrains[0][2] = true;			
				} 

				startPos_x = (start_long - mapCenterLong) * metersPerDegreeLong;
				startPos_z = (start_lat - mapCenterLat) * metersPerDegreeLat;
				printf("Not a full rebuild: startPos %f,%f  playerPos %f,%f startLong/Lat %f,%f\n",
					startPos_x,startPos_z,playerPos_x,playerPos_z,start_long,start_lat);
			}
			float heightSizeLong = tileWidthLong / (float)(heightmapRes-1);
			float heightSizeLat = tileWidthLat / (float)(heightmapRes-1);

			float textelSizeLong = tileWidthLong / (float)textureRes;
			float textelSizeLat = tileWidthLat / (float)textureRes;
			float textelSizeMeters = tileWidth / (float)textureRes;//redundant?

			//Now, if we have an whole multiple here we can grab height values as we go through the textures, 
			//otherwise they'll need their own loop.
			float texels_per_height_flt = (float)textureRes/(float)(heightmapRes-1);
			int texels_per_height = (int)texels_per_height_flt;
			if ((float)texels_per_height!=texels_per_height_flt)
				texels_per_height = 0;//If it's zero then it didn't work.

			//float start_long = player_long - (tileWidthLong * 1.5);//For the 5x5 tile area.
			//float start_lat = player_lat - (tileWidthLat * 1.5);
			//int totalTextureSize = textureRes * 3;
			//int totalHeightmapSize = ((heightmapRes-1) * 3) + 1;//(Because they share edges.)
			int totalTextureSize = textureRes;
			int totalHeightmapSize = heightmapRes;

			printf("TILE WIDTH %f, heightmapRes %d, textureRes %d tileWidthLong %f textelSizeLong %f  totaltexturesize %d\n",
				tileWidth,heightmapRes,textureRes,tileWidthLong,textelSizeLong,totalTextureSize);
			
			const int texture_array_size = totalTextureSize * totalTextureSize;
			int array_pos = 0;
			int bytes_remaining = texture_array_size;
			int linenum = 0;
			char *textureBuffer;
			textureBuffer = new char[texture_array_size];

			int num_height_bin_args = 5;// lat, long, tileWidth, heightmapRes, textureRes
			const int height_array_size = (totalHeightmapSize * totalHeightmapSize) + num_height_bin_args;
			float *heightBuffer;
			char *heightCharBuffer;
			heightBuffer = new float[height_array_size];
			char tempChars[255];
			FGScenery *kScenery = globals->get_scenery();
            /*
			//Here, change start_lat and start_long for each tile.
			for (int x=0;x<3;x++) 
			{
				for (int y=0;y<3;y++) 
				{
					start_long = base_long + (x * tileWidthLong);
					start_lat = base_lat + (y * tileWidthLat);
					
					bool textExists = false;
					sprintf(tempChars,"text.%s.bin",getTileName(start_long,start_lat).c_str());
					std::string textureBinfilePath = resourcePath + tempChars;
					fp = fopen(textureBinfilePath.c_str(),"r");
					if (fp) {
						textExists = true;
						printf("Texture file exists!  %s\n",textureBinfilePath.c_str());
						fclose(fp);
					}

					bool hghtExists = false;	
					sprintf(tempChars,"hght.%s.bin",getTileName(start_long,start_lat).c_str());
					std::string heightBinfilePath = resourcePath + tempChars;
					fp = fopen(heightBinfilePath.c_str(),"r");
					if (fp) {
						hghtExists = true;
						printf("Height file exists!  %s\n",heightBinfilePath.c_str());
						fclose(fp);
					}
					if ((mLoadTerrains[y][x])&&(!textExists)&&(!hghtExists))
					{				
						printf("trying to load terrain: %d %d",y,x);
						kPos.setLatitudeDeg(start_lat );
						kPos.setLongitudeDeg(start_long );
						kPos.setElevationM(0);

						heightBuffer[0] = start_long;
						heightBuffer[1] = start_lat;
						heightBuffer[2] = tileWidth;
						heightBuffer[3] = (float)heightmapRes;
						heightBuffer[4] = (float)textureRes;

						vector<string> unidentified_textures;
						
						//HERE:  Now we have to decide whether to load based on mLoadTerrains[][], instead of loading whole area.
						for (int i=0;i<totalTextureSize;i++)
						{
							for (int j=0;j<totalTextureSize;j++)
							{
								elevation_m = 0.0f;
								kPos.setLatitudeDeg(start_lat + (j*textelSizeLat));
								kPos.setLongitudeDeg(start_long + (i*textelSizeLong));
								kPos.setElevationM(0);
								//printf("looking for elevation %d %d: %f %f\n",j,i,start_lat + (j*textelSizeLat),start_long + (i*textelSizeLong));
								if (kScenery->get_elevation_m(SGGeod::fromGeodM(kPos, 3000),
									elevation_m, &bvhMat, 0))
								{
									mat = dynamic_cast<const SGMaterial *>(bvhMat);
									if (false)//((texels_per_height)&&(i%texels_per_height==0)&&(j%texels_per_height==0))
									{//Here:  if we are right on a line, then save the height data.
										heightBuffer[((j/texels_per_height)*totalHeightmapSize)+(i/texels_per_height)+num_height_bin_args] = elevation_m;
									}
									if (mat) 
									{
										if (!strcmp(mat->get_names()[0].c_str(),"BuiltUpCover"))
											textureBuffer[(j*totalTextureSize)+i]=(char)1;
										else if (!strcmp(mat->get_names()[0].c_str(),"EvergreenBroadCover"))
											textureBuffer[(j*totalTextureSize)+i]=(char)2;
										else if (!strcmp(mat->get_names()[0].c_str(),"DryCropPastureCover"))
											textureBuffer[(j*totalTextureSize)+i]=(char)3;
										else if (!strcmp(mat->get_names()[0].c_str(),"Grass"))
											textureBuffer[(j*totalTextureSize)+i]=(char)4;
										else if (!strcmp(mat->get_names()[0].c_str(),"Sand"))
											textureBuffer[(j*totalTextureSize)+i]=(char)5;
										else if ((!strcmp(mat->get_names()[0].c_str(),"Lake"))||
											(!strcmp(mat->get_names()[0].c_str(),"Pond"))||
											(!strcmp(mat->get_names()[0].c_str(),"Reservoir"))||
											(!strcmp(mat->get_names()[0].c_str(),"Stream"))||
											(!strcmp(mat->get_names()[0].c_str(),"Canal"))||
											(!strcmp(mat->get_names()[0].c_str(),"Lagoon"))||
											(!strcmp(mat->get_names()[0].c_str(),"Estuary"))||
											(!strcmp(mat->get_names()[0].c_str(),"Watercourse"))||
											(!strcmp(mat->get_names()[0].c_str(),"Saline")))
											textureBuffer[(j*totalTextureSize)+i]=(char)6;
										else 
										{
											bool newUnID = true;
											for (int k=0;k<unidentified_textures.size();k++)
											{
												if (!strcmp(unidentified_textures[k].c_str(),mat->get_names()[0].c_str()))
													newUnID = false;
											}
											if (newUnID)
											{
												unidentified_textures.insert(unidentified_textures.begin(),mat->get_names()[0]);
												printf("Unidentified texture: %s\n",mat->get_names()[0].c_str());
											}

											textureBuffer[(j*totalTextureSize)+i]=(char)0;
										}
									} else {
										textureBuffer[(j*totalTextureSize)+i]=(char)0;
									}
								}
							}
						}
						if (true)//(texels_per_height==0)
						{
							for (int i=0;i<totalHeightmapSize;i++)
							{
								for (int j=0;j<totalHeightmapSize;j++)
								{
									elevation_m = 0.0f;
									kPos.setLatitudeDeg(start_lat + (j*heightSizeLat));
									kPos.setLongitudeDeg(start_long + (i*heightSizeLong));
									kPos.setElevationM(0);
									if (kScenery->get_elevation_m(SGGeod::fromGeodM(kPos, 3000),
										elevation_m, &bvhMat, 0))
									{
										heightBuffer[(j*totalHeightmapSize)+i+num_height_bin_args] = elevation_m;
									}
								}
							}
						}
						//printf("FOUND ALL THE MATERIALS! %s  total size %d\n",mat->get_names()[0].c_str(),totalTextureSize*totalTextureSize);

						FILE *fp = NULL;
						//std::string textureBinfilePath = resourcePath + "terrain.textures.bin";
						//sprintf(tempChars,"terrain_%d_%d.textures.bin",y,x);
						printf("Writing to texture bin file: %s\n", textureBinfilePath.c_str());
						if (!(fp = fopen(textureBinfilePath.c_str(),"wb")))
							return;		
						fwrite(textureBuffer,texture_array_size,1,fp);
						fclose(fp);

						//std::string heightBinfilePath = resourcePath + "terrain.heights.bin";
						//sprintf(tempChars,"terrain_%d_%d.heights.bin",y,x);
						printf("Writing to heights bin file: %s  start lat/long %f  %f \n", 
							heightBinfilePath.c_str(),start_lat,start_long);
						if (!(fp = fopen(heightBinfilePath.c_str(),"wb")))
							return;		
						heightCharBuffer = reinterpret_cast<char*>(heightBuffer);
						fwrite(heightCharBuffer,height_array_size*sizeof(float),1,fp);
						fclose(fp);		
					}
				}
			}
			printf("made the files! texels_per_height %d  startPos %f,%f \n",
				texels_per_height,startPos_x,startPos_z);
			
			printf("removing terrain lockfile: %s\n",terrainLockfilePath.c_str());
			remove(terrainLockfilePath.c_str());

			//for (int i=0;i<3;i++)
			//{
			//	for (int j=0;j<3;j++)
			//	{
			//		fp = fopen(textureBinfilePath.c_str(),"wb");
			//		fclose(fp);
			//		fp = fopen(heightBinfilePath.c_str(),"wb");
			//		fclose(fp);
			//	}
			//}

			if (!mFullRebuild)
				return;
		}









        
		///////////////////// SKYBOXES /////////////////////////
		//Now, if we didn't do that, or if we are doing a full rebuild, check to see if we need to do skyboxes.
		if ((mFullRebuild)||((fabs(player_long-playerLastPos_long)*metersPerDegreeLong)>skyboxDistance)||
			((fabs(player_lat-playerLastPos_lat)*metersPerDegreeLat)>skyboxDistance))
		{//If we've gone more than skyboxDistance in either direction since last skybox, or we're fullRebuild, do another skybox.
			printf("playerlong = %f, lastLong %f, playerlat %f, lastlat %f, long-long=%f, lat-lat %f",
				player_long,playerLastPos_long,player_lat,playerLastPos_lat,
				(fabs(player_long-playerLastPos_long)*metersPerDegreeLong),
				(fabs(player_lat-playerLastPos_lat)*metersPerDegreeLat));
			playerLastPos_long = player_long;
			playerLastPos_lat = player_lat;
			//And, save the starting lat/long and resolution details in another file, so we know 
			//where to put the terrain tiles relative to the player.

			//NOW:  make a temporary lockfile, which will be removed when the last screenshot is done, so the
			//EDIT: instead, do this in Unity, on startup and whenever we reload, so we only need to delete it in FG.
			//RE-EDIT: no, DO make this here, because existence is now the signal to stop hitting us with requests.
			skyboxLockfilePath = resourcePath + "lockfile.skybox.tmp";// Unity end knows when it is safe to update skybox.
			fwp = fopen(skyboxLockfilePath.c_str(),"w");
			fclose(fwp);

			//screenshot_files[0] = "skybox3_00.png";//Maybe get these names from Unity client, or at least the 
			//screenshot_files[1] = "skybox3_270.png";// base name "skybox3", we could fill in _00.png, etc.
			//screenshot_files[2] = "skybox3_180.png";
			//screenshot_files[3] = "skybox3_90.png";
			screenshot_files[0] = "skybox3_270.png";
			screenshot_files[1] = "skybox3_00.png";
			screenshot_files[2] = "skybox3_90.png";
			screenshot_files[3] = "skybox3_180.png";
			screenshot_files[4] = "skybox3_up.png";

			printf("Requested longitude: %f  latitude: %f  elevation: %f path %s\n",
				player_long,player_lat,player_elev,resourcePathArray);

			int baseTextureRes = 128;
			float tileSize = 640.0f;//FIX
			int heightmapSize = 65;

			//double elevation_m = 0;
			elevation_m = 0;
			//SGGeod kPos;

			kPos.setLatitudeDeg(player_lat);
			kPos.setLongitudeDeg(player_long);
			kPos.setElevationM(0);

			//const simgear::BVHMaterial* bvhMat;
			if (globals->get_scenery()->get_elevation_m(SGGeod::fromGeodM(kPos, 3000),
				elevation_m, &bvhMat, 0))
			{
				const SGMaterial *mat2 = dynamic_cast<const SGMaterial *>(bvhMat);
				printf("Found elevation: %f\n",elevation_m);
				//if (mat2) 
				//{
				//	for (int i=0;i<mat2->get_names().size();i++)
				//	{
				//		printf("mat2 names[%d] %s\n",i,mat2->get_names()[i].c_str());
				//	}
				//}
			}

			player_elev = elevation_m + 30.0f;//computed position plus twenty meters.


			//osg::Camera* guiCamera = getGUICamera(flightgear::CameraGroup::getDefault());
			//osg::Matrixd viewMatrix;// = new osg::Matrixd();
			//osg::Vec3f camEye,camCenter,camUp;
			//guiCamera->getViewMatrixAsLookAt(camEye,camCenter,camUp);
			//const osg::View myView = guiCamera->getView();
			//viewMatrix = guiCamera->getViewMatrix();
			//viewPos = viewMatrix.getTrans();
			//printf("current view pos: %f %f %f\n",camEye.x(),camEye.y(),camEye.z());
			//viewPos.set(player_long,400.0f,player_lat);
			//viewMatrix.setTrans(viewPos);
			//guiCamera->setViewMatrix(viewMatrix);

			//OR, try this using the Property Tree:
			fgSetDouble("/position/longitude-deg",player_long);
			fgSetDouble("/position/latitude-deg",player_lat);

			//HERE: we need raycast down to find terrain elevation, and add a few feet.
			fgSetDouble("/position/altitude-ft",player_elev * (39.0f/12.0f));//NOTE: for flightgear this is in FEET, not meters.
			fgSetDouble("/orientation/heading-deg",0.0f);//IMPORTANT!  If you don't do this then all your skyboxes will be off.
			//(Unfortunately, it doesn't work???)
			fgSetDouble("/sim/current-view/goal-heading-offset-deg",0.0f);

			//fgSetDouble("/position/ground-elev-m",1.0f);//OR, maybe this works, to be 10m above the ground??
			fgSetDouble("/sim/current-view/field-of-view",90.0f);//Would it be faster to only do these once?
			fgSetBool("/sim/rendering/shaders/skydome",true);
			printf("current longitude: %f  latitude %f  skydome  %s\n",fgGetDouble("/position/longitude-deg"),fgGetDouble("/position/latitude-deg"),
				fgGetString("/sim/rendering/shaders/skydome"));

			//fgDumpSnapShot();//But this first time, just take the picture.

			skyboxStage = 1;

			//skyboxLoopInterval = 40;//10;//Mm, need precise timing mechanism here that knows when the last step is done.
		}
	}
}



/////  CULL - NOT USEFUL AT THIS TIME  ///////////////


void skyboxSocketDraw()
{
	printf("starting skybox draw.\n");//  skyboxReady %d\n",skyboxReady);
	//UPDATE: Now we are doing this all in ONE snapshot, with five cameras in it, and breaking them up.
	////NOW, more logic.  Would be nice to do a clean loop here but for now, five hard coded situations
	////will suffice.  Basically, we need to dump five snapshots, and after each one go away and on the
	////next time back, send those snapshots over the wire to the client.
	//if (skyboxReady==false)
	//{
	//	//fgHiResDump();//apparently not available?
	//	fgDumpSnapShot();
	//	skyboxReady = true;
	//	return;
	//}

	char *imgBuffer;
	int *imgInts;

	//if (screenshotFront.length()==0)
	//  screenshotFront = screenshotPath;
	//else if 

	//screenshotPath = "/home/chris/projects/flightgear/revert/flightgear_build/src/Main/fgfs-screen-004.png";
	//printf("Screenshot path:  %s\n",screenshotPath.c_str());
	//osgDB::fstream fs;
	float data;
	//char *terrBuffer;
	const int packetsize = 1024;//Turns out this isn't necessary as an actual packet size, TCP handles that.
	char buffer[packetsize];//,buffer2[packetsize];//I do need a number for my input buffer size though.
	FILE *fp = NULL;

	//struct stat results;
	//if (stat(screenshotPath.c_str(),&results))
	//{
	// = results.st_size;
	//fs.open(screenshotPath.c_str(),std::ios_base::in);
	//while (fp==NULL)//Whoops, we need to give the other thread(?) some time here to finish writing the image file.
	//  fp = fopen(screenshotPath.c_str(),"rb");//Anything that locks us in a loop here takes over the processor and
	//we never will finish, so I guess we need to check back in the next update.
	if (!(fp = fopen(screenshotPath.c_str(),"rb")))
		return;
	else
	{
		fseek(fp,0,SEEK_END);
		int fileSize = ftell(fp);
		printf("Drawing skybox file,  size:  %d\n",fileSize);
		rewind(fp);

		//int array_size = sizeof(int) * 1;
		//imgInts = new int[1];
		//imgInts[0] = fileSize;
		//imgBuffer = reinterpret_cast<char*>(imgInts);

		//n = send(newsockfd,imgBuffer,array_size,0);
		//if (n < 0) printf("ERROR writing to socket");
		//else printf("sent %d chars!\n",array_size);
	
		imgBuffer = new char[fileSize];
		//if (fs.is_open()) 
		//{
		//fs.read(imgBuffer,fileSize);
		//printf("We apparently loaded the file!  size %d",fileSize);
		//fs.close();	
		

		fread(imgBuffer,fileSize,1,fp);
		fclose(fp);

		osg::Image *image = osgDB::readImageFile(screenshotPath.c_str());
		//printf("loaded image! name %s file %s  size %d  width %d height %d data %d %d %d\n",image->getName().c_str(),
		//	image->getFileName(),image->getImageSizeInBytes(),image->s(),image->t(),
		//	image->data(0,0),image->data(0,1),image->data(0,2));

		//Debugging, save whole source image with all five camera views.
		//skyboxPath = resourcePath + "skybox_src.png";
		//FILE *fwp = fopen(skyboxPath.c_str(),"wb");
		//fwrite(imgBuffer,fileSize,1,fwp);
		//fclose(fwp);

		////HERE: write to the screenshot_files instead of sending across the socket.
		int srcWidth = 4000;//FIX!!! Get all these from client.
		int subWidth = 800;
		int subHeight = 800;//Could probably assume a square, but...
		for (int i=0;i<5;i++)
		{			
			osg::Vec3b* srcDataPtr = (osg::Vec3b*)image->data();
			osg::Image *subImage = new osg::Image;
			subImage->allocateImage(subWidth,subHeight,1,GL_RGB,GL_UNSIGNED_BYTE);
			subImage->setInternalTextureFormat(GL_RGB);
			osg::Vec3b* dataPtr = (osg::Vec3b*)subImage->data();
			for (int y=0;y<subHeight;y++)
			{
				memcpy(dataPtr,srcDataPtr+(i*subWidth),subWidth*3);
				dataPtr += subWidth;
				srcDataPtr += srcWidth;
			}
			skyboxPath = resourcePath + screenshot_files[i];
			subImage->setFileName(skyboxPath);
			osgDB::writeImageFile(*subImage,skyboxPath);
            subImage->unref();
		}

		remove(screenshotPath.c_str());//And then, DELETE IT so we don't flood the directory and break at _999 + 1.
		image->unref();		
				
		if (mFullRebuild)
		{
			mFullRebuild = false;
		}
		remove(skyboxLockfilePath.c_str());//Now that we're done, delete the tmp file so Unity knows it can reload skyboxes.
		closesocket(newsockfd);
		delete [] imgBuffer;
		printf("Leaving skybox draw, stage %d loopInterval %d\n",skyboxStage,skyboxLoopInterval);		
	} 
	
}



/////  CULL - FULLY OBSOLETE  ///////////////////////

void skyboxSocketConnect()
{
   const int packetsize = 1024;//Turns out this isn't necessary as an actual packet size, TCP handles that.
   unsigned char buffer[packetsize],buffer2[packetsize];//I do need a number for my input buffer size though.
   
   struct sockaddr_in serv_addr, cli_addr, terr_serv_addr, terr_cli_addr;
   int n;

   //TESTING... nonblocking select set
   struct timeval tv,terr_tv;//just in case we're modifying this in any way...
   SG_LOG(SG_GENERAL, SG_INFO,"dataSource: Trying to connect skyboxSocket!!!!!!!");

   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) 
     printf("ERROR opening socket\n");
   else 
     SG_LOG(SG_GENERAL, SG_INFO, "dataSource: SUCCESS opening Socket!!!!!!!");
      
   //terrSock = socket(AF_INET, SOCK_STREAM, 0);
   //if (terrSock < 0) 
   //  printf("ERROR opening terrSock\n");
   //else 
   //  printf("SUCCESS opening terrSock\n");

   ///////////////////
   //int yes=1;
   BOOL bOptVal = TRUE;
   //// lose the pesky "Address already in use" error message
   if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char *) &bOptVal,sizeof(BOOL)) == -1) {
     perror("setsockopt");
     exit(1);
   } 
   //if (setsockopt(terrSock,SOL_SOCKET,SO_REUSEADDR,(char *) &bOptVal,sizeof(BOOL)) == -1) {
   //  perror("setsockopt");
   //  exit(1);
   //} 

   SG_LOG(SG_GENERAL, SG_INFO, "dataSource: Setting up buffers...");
   
   //bzero((char *) &serv_addr, sizeof(serv_addr));
   //portno = 9934;//atoi(argv[1]);
   ZeroMemory(&serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = inet_addr("10.0.0.218");//INADDR_ANY;
   serv_addr.sin_port = htons(skyboxPort);
   
   bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr));
     //printf("ERROR on binding sockfd");

   //ZeroMemory(&terr_serv_addr, sizeof(terr_serv_addr));
   //terr_serv_addr.sin_family = AF_INET;
   //terr_serv_addr.sin_addr.s_addr = INADDR_ANY;
   //terr_serv_addr.sin_port = htons(terrainPort);
   //
   //if (bind(terrSock, (struct sockaddr *) &terr_serv_addr,
	  //  sizeof(terr_serv_addr)) < 0) 
   //  printf("ERROR on binding terrSock");

   
   //int select(int numfds, fd_set *readfds, fd_set *writefds,
   //        fd_set *exceptfds, struct timeval *timeout); 

   //listen(sockfd,5);//5 is backlog number, might want more if we get busy.


   FD_ZERO(&master);
   FD_ZERO(&readfds);
   FD_SET(sockfd, &master);

   tv.tv_sec = sockTimeout;
   tv.tv_usec = 0;

   readfds = master;
   select(sockfd+1, &readfds, NULL, NULL, &tv);
     
   SG_LOG(SG_GENERAL, SG_INFO, "dataSource: Socket setup finished!!!!!!!");
   //terr_tv.tv_sec = sockTimeout;//Just in case it's getting modified, &tv makes me nervous...
   //terr_tv.tv_usec = 0;
   //
   //FD_ZERO(&terr_master);
   //FD_ZERO(&terr_readfds);
   //FD_SET(terrSock, &terr_master);
   // 
   //terr_readfds = terr_master;
   //select(terrSock+1, &terr_readfds, NULL, NULL, &terr_tv);
}*/
