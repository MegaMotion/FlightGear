// SwiftAircraft.cpp - Class representing an aircraft generated by swift 
// 
// Copyright (C) 2019 - swift Project Community / Contributors (http://swift-project.org/)
// Adapted to Flightgear by Lars Toenning <dev@ltoenning.de>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <string.h>

#include <simgear/compiler.h>

#include <boost/foreach.hpp>
#include <string>

#include <osg/Node>
#include <osg/ref_ptr>
#include <osgDB/FileUtils>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>
#include <simgear/scene/model/modellib.hxx>
#include <simgear/scene/util/SGNodeMasks.hxx>

#include "SwiftAircraft.h"
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Scenery/scenery.hxx>
#include <Scripting/NasalModelData.hxx>
#include <Scripting/NasalSys.hxx>
#include <Sound/fg_fx.hxx>

FGSwiftAircraft::FGSwiftAircraft(std::string callsign, std::string modelpath)
{
    using namespace simgear;
    _model = SGModelLib::loadModel(modelpath);
    _model->setName(callsign);
    if (_model.valid()) {
        aip.init(_model);
        aip.setVisible(true);
        aip.update();
        globals->get_scenery()->get_models_branch()->addChild(aip.getSceneGraph());
    }
}

bool FGSwiftAircraft::updatePosition(SGGeod newPosition, SGVec3d orientation)
{

    position = newPosition;
    aip.setPosition(position);
    aip.setPitchDeg(orientation.x());
    aip.setRollDeg(orientation.y());
    aip.setHeadingDeg(orientation.z());
    aip.update();
    return true;
}


FGSwiftAircraft::~FGSwiftAircraft()
{
    aip.setVisible(false);
}

double FGSwiftAircraft::getLatDeg() 
{ 
	return position.getLatitudeDeg();
}

double FGSwiftAircraft::getLongDeg() 
{ 
	return position.getLongitudeDeg();
}

double FGSwiftAircraft::getFudgeFactor() 
{ 
	return 0; 
}

inline bool FGSwiftAircraft::operator<(std::string extCallsign)
{
    return _model->getName().compare(extCallsign);
}

double FGSwiftAircraft::getGroundElevation()
{
    double alt;
    globals->get_scenery()->get_elevation_m(position,alt,0);
    return alt; 
		
}