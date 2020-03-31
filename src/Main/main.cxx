// main.cxx -- top level sim routines
//
// Written by Curtis Olson, started May 1997.
//
// Copyright (C) 1997 - 2002  Curtis L. Olson  - http://www.flightgear.org/~curt
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
//
// $Id$


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <simgear/compiler.h>
#include <simgear/props/props_io.hxx>

#include <iostream>

//Chris Calef - dataSource library
//End dataSource

#include <osg/Camera>
#include <osg/GraphicsContext>
#include <osgDB/Registry>

#if defined(HAVE_CRASHRPT)
#include <CrashRpt.h>

// defined in bootstrap.cxx
extern bool global_crashRptEnabled;

#endif

// Class references
#include <simgear/canvas/VGInitOperation.hxx>
#include <simgear/emesary/Emesary.hxx>
#include <simgear/emesary/notifications.hxx>
#include <simgear/io/raw_socket.hxx>
#include <simgear/math/SGMath.hxx>
#include <simgear/math/sg_random.h>
#include <simgear/misc/strutils.hxx>
#include <simgear/props/AtomicChangeListener.hxx>
#include <simgear/props/props.hxx>
#include <simgear/scene/material/Effect.hxx>
#include <simgear/scene/material/mat.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/model/modellib.hxx>
#include <simgear/scene/tsync/terrasync.hxx>
#include <simgear/structure/commands.hxx>
#include <simgear/timing/sg_time.hxx>

#include <Add-ons/AddonManager.hxx>
#include <GUI/MessageBox.hxx>
#include <GUI/gui.h>
#include <Include/build.h>
#include <Include/version.h>
#include <Main/locale.hxx>
#include <Model/panelnode.hxx>
#include <Navaids/NavDataCache.hxx>
#include <Scenery/scenery.hxx>
#include <Sound/soundmanager.hxx>
#include <Time/TimeManager.hxx>
#include <Viewer/WindowSystemAdapter.hxx>
#include <Viewer/renderer.hxx>
#include <Viewer/splash.hxx>

#include "fg_commands.hxx"
#include "fg_init.hxx"
#include "fg_io.hxx"
#include "fg_os.hxx"
#include "fg_props.hxx"
#include "main.hxx"
#include "options.hxx"
#include "positioninit.hxx"
#include "screensaver_control.hxx"
#include "subsystemFactory.hxx"
#include "util.hxx"

#include <EmbeddedResources/FlightGear-resources.hxx>
#include <simgear/embedded_resources/EmbeddedResourceManager.hxx>

#if defined(HAVE_QT)
#include <GUI/QtLauncher.hxx>
#endif

#ifdef __OpenBSD__
#include <sys/resource.h>
#endif

#include "controlDataSource.h"
#include "worldDataSource.h"

using namespace flightgear;

using std::cerr;
using std::vector;

// The atexit() function handler should know when the graphical subsystem
// is initialized.
extern int _bootstrap_OSInit;

static SGPropertyNode_ptr frame_signal;
static SGPropertyNode_ptr nasal_gc_threaded;
static SGPropertyNode_ptr nasal_gc_threaded_wait;

#ifdef NASAL_BACKGROUND_GC_THREAD
extern "C" {
extern void startNasalBackgroundGarbageCollection();
extern void stopNasalBackgroundGarbageCollection();
extern void performNasalBackgroundGarbageCollection();
extern void awaitNasalGarbageCollectionComplete(bool can_wait);
}
#endif
static simgear::Notifications::MainLoopNotification                             mln_begin(simgear::Notifications::MainLoopNotification::Type::Begin);
static simgear::Notifications::MainLoopNotification                             mln_end(simgear::Notifications::MainLoopNotification::Type::End);
static simgear::Notifications::MainLoopNotification                             mln_started(simgear::Notifications::MainLoopNotification::Type::Started);
static simgear::Notifications::MainLoopNotification                             mln_stopped(simgear::Notifications::MainLoopNotification::Type::Stopped);
static simgear::Notifications::NasalGarbageCollectionConfigurationNotification* ngccn = nullptr;
// This method is usually called after OSG has finished rendering a frame in what OSG calls an idle handler and
// is reposonsible for invoking all of the relevant per frame processing; most of which is handled by subsystems.


// Chris Calef - dataSource

//NEW DATASOURCE:


controlDataSource* mDataSource;
worldDataSource*   mWorldDataSource;


////////////////////////////////////////////////////////////////////////////////////////////////////////


// OLD SKYBOX SOCKET
/*
//int skyboxSockFD,newSkyboxSockFD,skyboxPortNo;
//if (unix)
//int sockfd=0;
//int newsockfd;
//else (windows)
SOCKET sockfd,newsockfd,terrSock,newTerrSock;
//end if (unix)
int skyboxPort = 9935;
//int terrainPort = 9936;
int sockTimeout=0;//seconds
int skyboxStage=0;
int skyboxNum=0;
int skyboxLoopInterval=3;//How many main loops we skip between socket listens.
//bool skyboxReady=false;
bool mFullRebuild=false;

fd_set master;
fd_set readfds;
fd_set terr_master;
fd_set terr_readfds;

std::string screenshotPath;
std::string resourcePath;
std::string skyboxPath;
std::string skyboxLockfilePath;
std::string terrainLockfilePath;
std::string screenshot_files[5];

double playerLastPos_long;
double playerLastPos_lat;

double mapCenterLong = -123.10;//FIX!!!  Get this in the client request.
double mapCenterLat = 44.00;

float skyboxDistance = 5.0f;

void skyboxSocketConnect();
void skyboxSocketListen();

//void skyboxSocketStart();
void skyboxSocketDraw();
//void terrainSocketListen();

std::string getTileName(double,double);

int mTick=0;//Better way to measure time?
*/


//End dataSource


static void fgMainLoop(void)
{
    //
    // the Nasal GC will automatically run when (during allocation) it discovers that more space is needed.
    // This has a cost of between 5ms and 50ms (depending on the amount of currently active Nasal).
    // The result is unscheduled and unpredictable pauses during normal operation when the garbage collector
    // runs; which typically occurs at intervals between 1sec and 20sec.
    //
    // The solution to this, which overall increases CPU load, is to use a thread to do this; as Nasal is thread safe
    // so what we do is to launch the garbage collection at the end of the main loop and then wait for completion at the start of the
    // next main loop.
    // So although the overall CPU is increased it has little effect on the frame rate; if anything it is an overall benefit
    // as there are no unscheduled long duration frames.
    //
    // The implementation appears to work fine without waiting for completion at the start of the frame - so
    // this wait at the start can be disabled by setting the property /sim/nasal-gc-threaded-wait to false.

    // first we see if the config has changed. The notification will return true from SetActive/SetWait when the
    // value has been changed - and thus we notify the Nasal system that it should configure itself accordingly.
    auto use_threaded_gc  = nasal_gc_threaded->getBoolValue();
    auto threaded_wait    = nasal_gc_threaded_wait->getBoolValue();
    bool notify_gc_config = false;
    notify_gc_config      = ngccn->SetActive(use_threaded_gc);
    notify_gc_config |= ngccn->SetWait(threaded_wait);
    if (notify_gc_config)
        simgear::Emesary::GlobalTransmitter::instance()->NotifyAll(*ngccn);

    simgear::Emesary::GlobalTransmitter::instance()->NotifyAll(mln_begin);

    if (sglog().has_popup()) {
        std::string s = sglog().get_popup();
        flightgear::modalMessageBox("Alert", s, "");
    }

    frame_signal->fireValueChanged();

    auto timeManager = globals->get_subsystem<TimeManager>();
    // compute simulated time (allowing for pause, warp, etc) and
    // real elapsed time
    double sim_dt, real_dt;
    timeManager->computeTimeDeltas(sim_dt, real_dt);

    // update all subsystems
    globals->get_subsystem_mgr()->update(sim_dt);

    // flush commands waiting in the queue
    SGCommandMgr::instance()->executedQueuedCommands();
    simgear::AtomicChangeListener::fireChangeListeners();

    //Chris Calef - dataSource
    mDataSource->tick();
    mWorldDataSource->tick();

    simgear::Emesary::GlobalTransmitter::instance()->NotifyAll(mln_end);
}

static void initTerrasync()
{
    // add the terrasync root as a data path so data can be retrieved from it
    // (even if we are in read-only mode)
    SGPath terraSyncDir(globals->get_terrasync_dir());
    globals->append_data_path(terraSyncDir);

    if (fgGetBool("/sim/fghome-readonly", false)) {
        return;
    }

    // start TerraSync up now, so it can be synchronizing shared models
    // and airports data in parallel with a nav-cache rebuild.
    SGPath tsyncCache(terraSyncDir);
    tsyncCache.append("terrasync-cache.xml");

    // wipe the cache file if requested
    if (flightgear::Options::sharedInstance()->isOptionSet("restore-defaults")) {
        SG_LOG(SG_GENERAL, SG_INFO, "restore-defaults requested, wiping terrasync update cache at " << tsyncCache);
        if (tsyncCache.exists()) {
            tsyncCache.remove();
        }
    }

    fgSetString("/sim/terrasync/cache-path", tsyncCache.utf8Str());

    // make fg-root dir available so existing Scenery data can be copied, and
    // hence not downloaded again.
    fgSetString("/sim/terrasync/installation-dir", (globals->get_fg_root() / "Scenery").utf8Str());

    simgear::SGTerraSync* terra_sync = new simgear::SGTerraSync();
    terra_sync->setRoot(globals->get_props());
    globals->add_subsystem("terrasync", terra_sync);

    terra_sync->bind();
    terra_sync->init();
}

static void fgSetVideoOptions()
{
    std::string vendor = fgGetString("/sim/rendering/gl-vendor");
    SGPath      path(globals->get_fg_root());
    path.append("Video");
    path.append(vendor);
    if (path.exists()) {
        std::string renderer = fgGetString("/sim/rendering/gl-renderer");
        size_t      pos      = renderer.find("x86/");
        if (pos == std::string::npos) {
            pos = renderer.find('/');
        }
        if (pos == std::string::npos) {
            pos = renderer.find(" (");
        }
        if (pos != std::string::npos) {
            renderer = renderer.substr(0, pos);
        }
        path.append(renderer + ".xml");
        if (path.exists()) {
            SG_LOG(SG_INPUT, SG_INFO, "Reading video settings from " << path);
            try {
                SGPropertyNode* r_prop = fgGetNode("/sim/rendering");
                readProperties(path, r_prop);
            } catch (sg_exception& e) {
                SG_LOG(SG_INPUT, SG_WARN, "failed to read video settings:" << e.getMessage() << "(from " << e.getOrigin() << ")");
            }

            flightgear::Options* options = flightgear::Options::sharedInstance();
            if (!options->isOptionSet("ignore-autosave")) {
                globals->loadUserSettings();
            }
        }
    }
}

static void checkOpenGLVersion()
{
#if defined(SG_MAC)
    // Mac users can't upgrade their drivers, so complaining about
    // versions doesn't help them much
    return;
#endif

    // format of these strings is not standardised, so be careful about
    // parsing them.
    std::string versionString(fgGetString("/sim/rendering/gl-version"));
    string_list parts = simgear::strutils::split(versionString);
    if (parts.size() == 3) {
        if (parts[1].find("NVIDIA") != std::string::npos) {
            // driver version number, dot-seperared
            string_list driverVersion = simgear::strutils::split(parts[2], ".");
            if (!driverVersion.empty()) {
                int majorDriverVersion = simgear::strutils::to_int(driverVersion[0]);
                if (majorDriverVersion < 300) {
                    std::ostringstream ss;
                    ss << "Please upgrade to at least version 300 of the nVidia drivers (installed version is " << parts[2] << ")";

                    flightgear::modalMessageBox("Outdated graphics drivers",
                                                "FlightGear has detected outdated drivers for your graphics card.",
                                                ss.str());
                }
            }
        } // of NVIDIA-style version string
    }     // of three parts
}

static void registerMainLoop()
{
    // stash current frame signal property
    frame_signal           = fgGetNode("/sim/signals/frame", true);
    nasal_gc_threaded      = fgGetNode("/sim/nasal-gc-threaded", true);
    nasal_gc_threaded_wait = fgGetNode("/sim/nasal-gc-threaded-wait", true);
    fgRegisterIdleHandler(fgMainLoop);

    SG_LOG(SG_GENERAL, SG_INFO, "CALLING REGISTER MAIN LOOP!!!");
    mDataSource      = new controlDataSource(true, true, 9985, "127.0.0.1");
    mWorldDataSource = new worldDataSource(true, true, 9987, "127.0.0.1");
    //skyboxSocketConnect();//Chris Calef - dataSource
}

// This is the top level master main function that is registered as
// our idle function

// The first few passes take care of initialization things (a couple
// per pass) and once everything has been initialized fgMainLoop from
// then on.

static int idle_state = 0;

static void fgIdleFunction(void)
{
    // Specify our current idle function state.  This is used to run all
    // our initializations out of the idle callback so that we can get a
    // splash screen up and running right away.

    if (idle_state == 0) {
        if (guiInit()) {
            checkOpenGLVersion();
            fgSetVideoOptions();
            idle_state += 2;
            fgSplashProgress("loading-aircraft-list");
            fgSetBool("/sim/rendering/initialized", true);
        }

    } else if (idle_state == 2) {
        initTerrasync();
        idle_state++;
        fgSplashProgress("loading-nav-dat");

    } else if (idle_state == 3) {
        bool done = fgInitNav();
        if (done) {
            ++idle_state;
            fgSplashProgress("init-scenery");
        }
    } else if (idle_state == 4) {
        idle_state++;

        globals->add_new_subsystem<TimeManager>(SGSubsystemMgr::INIT);

        // Do some quick general initializations
        if (!fgInitGeneral()) {
            throw sg_exception("General initialization failed");
        }

        ////////////////////////////////////////////////////////////////////
        // Initialize the property-based built-in commands
        ////////////////////////////////////////////////////////////////////
        fgInitCommands();
        fgInitSceneCommands();

        flightgear::registerSubsystemCommands(globals->get_commands());

        ////////////////////////////////////////////////////////////////////
        // Initialize the material manager
        ////////////////////////////////////////////////////////////////////
        globals->set_matlib(new SGMaterialLib);
        simgear::SGModelLib::setPanelFunc(FGPanelNode::load);

    } else if ((idle_state == 5) || (idle_state == 2005)) {
        idle_state += 2;
        flightgear::initPosition();
        flightgear::initTowerLocationListener();

        simgear::SGModelLib::init(globals->get_fg_root().local8BitStr(), globals->get_props());

        auto timeManager = globals->get_subsystem<TimeManager>();
        timeManager->init();

        ////////////////////////////////////////////////////////////////////
        // Initialize the TG scenery subsystem.
        ////////////////////////////////////////////////////////////////////

        globals->add_new_subsystem<FGScenery>(SGSubsystemMgr::DISPLAY);
        globals->get_scenery()->init();
        globals->get_scenery()->bind();

        fgSplashProgress("creating-subsystems");
    } else if ((idle_state == 7) || (idle_state == 2007)) {
        bool isReset = (idle_state == 2007);
        idle_state   = 8; // from the next state on, reset & startup are identical
        SGTimeStamp st;
        st.stamp();
        fgCreateSubsystems(isReset);
        SG_LOG(SG_GENERAL, SG_INFO, "Creating subsystems took:" << st.elapsedMSec());
        fgSplashProgress("binding-subsystems");

    } else if (idle_state == 8) {
        idle_state++;
        SGTimeStamp st;
        st.stamp();
        globals->get_subsystem_mgr()->bind();
        SG_LOG(SG_GENERAL, SG_INFO, "Binding subsystems took:" << st.elapsedMSec());

        fgSplashProgress("init-subsystems");
    } else if (idle_state == 9) {
        SGSubsystem::InitStatus status = globals->get_subsystem_mgr()->incrementalInit();
        if (status == SGSubsystem::INIT_DONE) {
            ++idle_state;
            fgSplashProgress("finishing-subsystems");
        } else {
            fgSplashProgress("init-subsystems");
        }

    } else if (idle_state == 10) {
        idle_state = 900;
        fgPostInitSubsystems();
        fgSplashProgress("finalize-position");
    } else if (idle_state == 900) {
        idle_state = 1000;

        // setup OpenGL view parameters
        globals->get_renderer()->setupView();

        globals->get_renderer()->resize(fgGetInt("/sim/startup/xsize"),
                                        fgGetInt("/sim/startup/ysize"));
        WindowSystemAdapter::getWSA()->windows[0]->gc->add(
            new simgear::canvas::VGInitOperation());

        int session = fgGetInt("/sim/session", 0);
        session++;
        fgSetInt("/sim/session", session);
    }

    if (idle_state == 1000) {
        sglog().setStartupLoggingEnabled(false);

        // We've finished all our initialization steps, from now on we
        // run the main loop.
        fgSetBool("sim/sceneryloaded", false);
        registerMainLoop();

        ngccn = new simgear::Notifications::NasalGarbageCollectionConfigurationNotification(nasal_gc_threaded->getBoolValue(), nasal_gc_threaded_wait->getBoolValue());
        simgear::Emesary::GlobalTransmitter::instance()->NotifyAll(*ngccn);
        simgear::Emesary::GlobalTransmitter::instance()->NotifyAll(mln_started);
    }

    if (idle_state == 2000) {
        fgStartNewReset();
        idle_state = 2005;
    }
}

void fgResetIdleState()
{
    idle_state = 2000;
    fgRegisterIdleHandler(&fgIdleFunction);
}

void fgInitSecureMode()
{
    bool secureMode = true;
    if (Options::sharedInstance()->isOptionSet("allow-nasal-from-sockets")) {
        SG_LOG(SG_GENERAL, SG_ALERT, "\n!! Network connections allowed to use Nasal !!\n"
                                     "Network connections will be allowed full access to the simulator \n"
                                     "including running arbitrary scripts. Ensure you have adequate security\n"
                                     "(such as a firewall which blocks external connections).\n");
        secureMode = false;
    }

    // it's by design that we overwrite any existing property tree value
    // here - this prevents an aircraft or add-on setting the property
    // value underneath us, eg in their -set.xml
    SGPropertyNode_ptr secureFlag = fgGetNode("/sim/secure-flag", true);
    secureFlag->setBoolValue(secureMode);
    secureFlag->setAttributes(SGPropertyNode::READ |
                              SGPropertyNode::PRESERVE |
                              SGPropertyNode::PROTECTED);
}

static void upper_case_property(const char* name)
{
    using namespace simgear;
    SGPropertyNode* p = fgGetNode(name, false);
    if (!p) {
        p = fgGetNode(name, true);
        p->setStringValue("");
    } else {
        props::Type t = p->getType();
        if (t == props::NONE || t == props::UNSPECIFIED)
            p->setStringValue("");
        else
            assert(t == props::STRING);
    }
    SGPropertyChangeListener* muc = new FGMakeUpperCase;
    globals->addListenerToCleanup(muc);
    p->addChangeListener(muc);
}

// this hack is needed to avoid weird viewport sizing within OSG on Windows.
// still required as of March 2017, sad times.
// see for example https://sourceforge.net/p/flightgear/codetickets/1958/
static void ATIScreenSizeHack()
{
    osg::ref_ptr<osg::Camera> hackCam = new osg::Camera;
    hackCam->setRenderOrder(osg::Camera::PRE_RENDER);
    int prettyMuchAnyInt = 1;
    hackCam->setViewport(0, 0, prettyMuchAnyInt, prettyMuchAnyInt);
    globals->get_renderer()->addCamera(hackCam, false);
}

// Propose NVIDIA Optimus / AMD Xpress to use high-end GPU
#if defined(SG_WINDOWS)
extern "C" {
_declspec(dllexport) DWORD NvOptimusEnablement                = 0x00000001;
_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

static void logToHome()
{
    SGPath logPath = globals->get_fg_home();
    logPath.append("fgfs.log");
    if (logPath.exists()) {
        SGPath prevLogPath = globals->get_fg_home();
        prevLogPath.append("fgfs_0.log");
        logPath.rename(prevLogPath);
        // bit strange, we need to restore the correct value of logPath now
        logPath = globals->get_fg_home();
        logPath.append("fgfs.log");
    }
    sglog().logToFile(logPath, SG_ALL, SG_INFO);

#if defined(HAVE_CRASHRPT)
    if (global_crashRptEnabled) {
        crAddFile2(logPath.c_str(), NULL, "FlightGear Log File", CR_AF_MAKE_FILE_COPY);
        SG_LOG(SG_GENERAL, SG_INFO, "CrashRpt enabled");
    } else {
        SG_LOG(SG_GENERAL, SG_WARN, "CrashRpt enabled at compile time but failed to install");
    }
#endif
}

// Main top level initialization
int fgMainInit(int argc, char** argv)
{
    // set default log level to 'info' for startup, we will revert to a lower
    // level once startup is done.
    sglog().setLogLevels(SG_ALL, SG_INFO);
    sglog().setStartupLoggingEnabled(true);

    globals = new FGGlobals;
    if (!fgInitHome()) {
        return EXIT_FAILURE;
    }

    const bool readOnlyFGHome = fgGetBool("/sim/fghome-readonly");
    if (!readOnlyFGHome) {
        // now home is initialised, we can log to a file inside it
        logToHome();
    }

    std::string version(FLIGHTGEAR_VERSION);
    SG_LOG(SG_GENERAL, SG_INFO, "FlightGear:  Version " << version);
    SG_LOG(SG_GENERAL, SG_INFO, "FlightGear:  Build Type " << FG_BUILD_TYPE);
    SG_LOG(SG_GENERAL, SG_INFO, "Built with " << SG_COMPILER_STR);
    SG_LOG(SG_GENERAL, SG_INFO, "Jenkins number/ID " << JENKINS_BUILD_NUMBER << ":" << JENKINS_BUILD_ID);

#if OSG_VERSION_LESS_THAN(3, 4, 1)
    SG_LOG(SG_GENERAL, SG_ALERT, "Minimum supported OpenScenegraph is V3.4.1 - currently using " << osgGetVersion() << " This can cause fatal OSG 'final reference count' errors at runtime");
#endif

#ifdef __OpenBSD__
    {
        /* OpenBSD defaults to a small maximum data segment, which can cause
        flightgear to crash with SIGBUS, so output a warning if this is likely.
        */
        struct rlimit rlimit;
        int           e = getrlimit(RLIMIT_DATA, &rlimit);
        if (e) {
            SG_LOG(SG_GENERAL, SG_INFO, "This is OpenBSD; getrlimit() failed: " << strerror(errno));
        } else {
            long long required = 4LL * (1LL << 30);
            if (rlimit.rlim_cur < required) {
                SG_LOG(SG_GENERAL, SG_POPUP, ""
                                                 << "Max data segment (" << rlimit.rlim_cur << "bytes) too small.\n"
                                                 << "This can cause Flightgear to crash due to SIGBUS.\n"
                                                 << "E.g. increase with 'ulimit -d " << required / 1024 << "'.");
            }
        }
    }
#endif

    // seed the random number generator
    sg_srandom_time();

    string_list* col = new string_list;
    globals->set_channel_options_list(col);

    fgValidatePath(globals->get_fg_home(), false); // initialize static variables
    upper_case_property("/sim/presets/airport-id");
    upper_case_property("/sim/presets/runway");
    upper_case_property("/sim/tower/airport-id");
    upper_case_property("/autopilot/route-manager/input");

    // check if the launcher is requested, since it affects config file parsing
    bool showLauncher = flightgear::Options::checkForArg(argc, argv, "launcher");
    // an Info.plist bundle can't define command line arguments, but it can set
    // environment variables. This avoids needed a wrapper shell-script on OS-X.
    showLauncher |= (::getenv("FG_LAUNCHER") != nullptr);
    if (showLauncher) {
        // to minimise strange interactions when launcher and config files
        // set overlaping options, we disable the default files. Users can
        // still explicitly request config files via --config options if they choose.
        flightgear::Options::sharedInstance()->setShouldLoadDefaultConfig(false);
    }

    if (showLauncher && readOnlyFGHome) {
        // this is perhaps not what the user wanted, let's inform them
        flightgear::modalMessageBox("Multiple copies of FlightGear",
                                    "Another copy of FlightGear is already running on this computer, "
                                    "so this copy will run in read-only mode.");
    }

    // Load the configuration parameters.  (Command line options
    // override config file options.  Config file options override
    // defaults.)
    int configResult = fgInitConfig(argc, argv, false);
    if (configResult == flightgear::FG_OPTIONS_ERROR) {
        return EXIT_FAILURE;
    } else if (configResult == flightgear::FG_OPTIONS_EXIT) {
        return EXIT_SUCCESS;
    }

#if defined(HAVE_QT)
    flightgear::initApp(argc, argv);
    if (showLauncher) {
        flightgear::checkKeyboardModifiersForSettingFGRoot();

        if (!flightgear::runLauncherDialog()) {
            return EXIT_SUCCESS;
        }
    }
#else
    if (showLauncher) {
        SG_LOG(SG_GENERAL, SG_ALERT, "\n!Launcher requested, but FlightGear was compiled without Qt support!\n");
    }
#endif

    fgInitSecureMode();
    fgInitAircraftPaths(false);

    configResult = fgInitAircraft(false);
    if (configResult == flightgear::FG_OPTIONS_ERROR) {
        return EXIT_FAILURE;
    } else if (configResult == flightgear::FG_OPTIONS_EXIT) {
        return EXIT_SUCCESS;
    }

    addons::AddonManager::createInstance();

    configResult = flightgear::Options::sharedInstance()->processOptions();
    if (configResult == flightgear::FG_OPTIONS_ERROR) {
        return EXIT_FAILURE;
    } else if (configResult == flightgear::FG_OPTIONS_EXIT) {
        return EXIT_SUCCESS;
    }

    // Set the lists of allowed paths for cases where a path comes from an
    // untrusted source, such as the global property tree (this uses $FG_HOME
    // and other paths set by Options::processOptions()).
    fgInitAllowedPaths();

    const auto& resMgr = simgear::EmbeddedResourceManager::createInstance();
    initFlightGearEmbeddedResources();
    // The language was set in processOptions()
    const std::string locale = globals->get_locale()->getPreferredLanguage();
    // Must always be done after all resources have been added to 'resMgr'
    resMgr->selectLocale(locale);
    SG_LOG(SG_GENERAL, SG_INFO,
           "EmbeddedResourceManager: selected locale '" << locale << "'");

    // Copy the property nodes for the menus added by registered add-ons
    addons::AddonManager::instance()->addAddonMenusToFGMenubar();

    // Initialize the Window/Graphics environment.
    fgOSInit(&argc, argv);
    _bootstrap_OSInit++;

    fgRegisterIdleHandler(&fgIdleFunction);

    // Initialize sockets (WinSock needs this)
    simgear::Socket::initSockets();

    // Clouds3D requires an alpha channel
    fgOSOpenWindow(true /* request stencil buffer */);
    fgOSResetProperties();

    fntInit();
    globals->get_renderer()->preinit();

    if (fgGetBool("/sim/ati-viewport-hack", true)) {
        SG_LOG(SG_GENERAL, SG_WARN, "Enabling ATI/AMD viewport hack");
        ATIScreenSizeHack();
    }

    fgOutputSettings();

    //try to disable the screensaver
    fgOSDisableScreensaver();

    // pass control off to the master event handler
    int result = fgOSMainLoop();
    frame_signal.clear();
    fgOSCloseWindow();

    simgear::Emesary::GlobalTransmitter::instance()->NotifyAll(mln_stopped);

    simgear::clearEffectCache();

    // clean up here; ensure we null globals to avoid
    // confusing the atexit() handler
    delete globals;
    globals = nullptr;

    // delete the NavCache here. This will cause the destruction of many cached
    // objects (eg, airports, navaids, runways).
    delete flightgear::NavDataCache::instance();

    return result;
}


/////////////////////////////////////////////////////////////////////////////////////////////
//  Chris Calef - dataSource - TEMP!!! If we keep this, then move it to new SkyboxSocket(?) class in its own file.


void exitProgramTemp()
{
    SG_LOG(SG_INPUT, SG_INFO, "exitProgramTemp: Program exit attempting...");
    fgSetBool("/sim/signals/exit", true); //Maybe?
    globals->saveUserSettings();
    fgOSExit(0);
}

DWORD returnTimeOfDay()
{
    return timeGetTime();
}

void doEngineStart()
{
    SG_LOG(SG_GENERAL, SG_INFO, "Doing engine start!!!!!!!!");

    int state = fgGetInt("/controls/engines/engine/magnetos");
    if (state == 0)
        fgSetInt("/controls/engines/engine/magnetos", 1);
    //state = fgGetInt("/controls/engines/engine/starter");
    //if (state == 0)
    //    fgSetInt("/controls/engines/engine/starter", 1);
}

void doAircraftReset()
{
    SG_LOG(SG_GENERAL, SG_INFO, "Doing aircraft reset!!!!!!!!");

    //Whoops! Have to kill our connection here, and restart it after, because we're going to lose it during the reset.
    delete mDataSource;
    delete mWorldDataSource;

    //Except doing this causes instant crash?? Uh oh

    fgResetIdleState();
}

void makeTerrainTile(worldDataSource* worldData, float startLong, float startLat, const char* tilename)
{
    char message[1024];

    double                      elevation_m = 0;
    SGGeod                      kPos;
    const simgear::BVHMaterial* bvhMat;
    const SGMaterial*           mat;

    float startPos_x = (startLong - worldData->mMapCenterLong) * worldData->mMetersPerDegreeLongitude;
    float startPos_z = (startLat - worldData->mMapCenterLat) * worldData->mMetersPerDegreeLatitude;


    float heightSizeLong = worldData->mTileWidthLong / (float)(worldData->mHeightMapRes - 1);
    float heightSizeLat  = worldData->mTileWidthLat / (float)(worldData->mHeightMapRes - 1);

    float textelSizeLong   = worldData->mTileWidthLong / (float)(worldData->mTextureRes);
    float textelSizeLat    = worldData->mTileWidthLat / (float)(worldData->mTextureRes);
    float textelSizeMeters = worldData->mTileWidth / (float)(worldData->mTextureRes); //redundant?


    float texels_per_height_flt = (float)(worldData->mTextureRes) / (float)(worldData->mHeightMapRes - 1);
    int   texels_per_height     = (int)texels_per_height_flt;
    if ((float)texels_per_height != texels_per_height_flt)
        texels_per_height = 0; //If it's zero then it didn't work.

    int totalTextureSize   = worldData->mTextureRes;
    int totalHeightmapSize = worldData->mHeightMapRes;

    //const int texture_array_size = totalTextureSize * totalTextureSize;
    const int texture_array_size = totalHeightmapSize * totalHeightmapSize; //How did I get this confused??? :-0
    int       array_pos          = 0;
    int       bytes_remaining    = texture_array_size;
    int       linenum            = 0;
    char*     textureBuffer;
    //textureBuffer = new char[texture_array_size];// Again, this used to work, now it doesn't, not sure why, but maybe hard coding will fix it
    //textureBuffer = new char[1048576];//until I figure out the right solution.
    //textureBuffer = (char*)malloc(texture_array_size);
    textureBuffer = (char*)malloc(texture_array_size); //OMFG how did I get so confused here??? Apparently the texture file should be only the size of the height array, *not* textureRes??? 


    int       num_height_bin_args = 5; // lat, long, tileWidth, heightmapRes, textureRes
    const int height_array_size   = (totalHeightmapSize * totalHeightmapSize) + num_height_bin_args;
    //float*    heightBuffer;
    //heightBuffer = new float[height_array_size];
    char* heightCharBuffer;
    float heightBuffer[65541];
    char  tempChars[255];

    FGScenery* kScenery = globals->get_scenery();

#ifdef windows_OS
    ////ZeroMemory((char*)&textureBuffer, sizeof(textureBuffer));//Wait, what??
    ////ZeroMemory((char*)&heightBuffer, sizeof(heightBuffer));
    //ZeroMemory((char*)&textureBuffer, texture_array_size * sizeof(char));
    //ZeroMemory((char*)&heightBuffer, height_array_size * sizeof(float));
#else
    bzero((char*)&textureBuffer, sizeof(textureBuffer));
    bzero((char*)&heightBuffer, sizeof(heightBuffer));
#endif

    sprintf(message, "makeTerrainTile - tileWidth %f  heightMapRes %d textureRes %d  sizeof texturebuffer %d sizeof heightbuffer %d",
            worldData->mTileWidth, worldData->mHeightMapRes, worldData->mTextureRes, sizeof(heightBuffer), sizeof(textureBuffer));
    SG_LOG(SG_GENERAL, SG_INFO, message);

    bool textExists = false;
    sprintf(tempChars, "text.%s.bin", tilename);
    std::string textureBinfilePath = worldData->mResourcePath + tempChars;
    FILE*       fp                 = fopen(textureBinfilePath.c_str(), "r");
    if (fp) {
        textExists = true;
        printf("Texture file exists!  %s\n", textureBinfilePath.c_str());
        fclose(fp);
    }

    bool hghtExists = false;
    sprintf(tempChars, "hght.%s.bin", tilename);
    std::string heightBinfilePath = worldData->mResourcePath + tempChars;
    fp                            = fopen(heightBinfilePath.c_str(), "r");
    if (fp) {
        hghtExists = true;
        printf("Height file exists!  %s\n", heightBinfilePath.c_str());
        fclose(fp);
    }


    sprintf(message, "trying to load terrain.... textureSize %d", totalTextureSize);
    SG_LOG(SG_GENERAL, SG_INFO, message);

    kPos.setLatitudeDeg(startLat);
    kPos.setLongitudeDeg(startLong);
    kPos.setElevationM(0);

    sprintf(message, "1 totalTextureSize %d", totalTextureSize);
    heightBuffer[0] = startLong;
    heightBuffer[1] = startLat;
    heightBuffer[2] = worldData->mTileWidth;
    heightBuffer[3] = (float)(worldData->mHeightMapRes);
    heightBuffer[4] = (float)(worldData->mTextureRes);

    vector<string> unidentified_textures;

    sprintf(message, "20000 totalTextureSize %d", totalTextureSize);
    SG_LOG(SG_GENERAL, SG_INFO, message);

    //sprintf(textureBuffer, "%d", 1);
    //sprintf(textureBuffer + 1, "%d", 2);

    SG_LOG(SG_GENERAL, SG_INFO, "Didn't write to textureBuffer...?");

    if (textureBuffer != NULL) 
    { 
        for (int i = 0; i < totalHeightmapSize; i++) {   //totalTextureSize
            for (int j = 0; j < totalHeightmapSize; j++) { //totalTextureSize
                elevation_m = 0.0f;
                kPos.setLatitudeDeg(startLat + (j * heightSizeLat));    //texelSizeLat
                kPos.setLongitudeDeg(startLong + (i * heightSizeLong)); //texelSizeLong
                kPos.setElevationM(0);
                //printf("looking for elevation %d %d: %f %f\n", j, i, startLat + (j * textelSizeLat), startLong + (i * textelSizeLong));
                //if ((i % 20 == 0) && (j % 20 == 0)) {
                //    sprintf(message, "looking for  elevation  %f %f", startLat + (j * textelSizeLat), startLong + (i * textelSizeLong));
                //    SG_LOG(SG_GENERAL, SG_INFO, message);
                //}
                //sprintf(message, "looking for elevation %d %d: %f %f", j, i, startLat + (j * textelSizeLat), startLong + (i * textelSizeLong));
                //SG_LOG(SG_GENERAL, SG_INFO, message);


                if (kScenery->get_elevation_m(SGGeod::fromGeodM(kPos, 3000),
                                              elevation_m, &bvhMat, 0)) {
                    mat = dynamic_cast<const SGMaterial*>(bvhMat);
                    //if ((i % 20 == 0 ) && (j % 20 == 0) )
                    //{
                    //    sprintf(message, "found elevation  %f", elevation_m);
                    //    SG_LOG(SG_GENERAL, SG_INFO, message);
                    //}
                    //if (false) //((texels_per_height)&&(i%texels_per_height==0)&&(j%texels_per_height==0))
                    //{          //Here:  if we are right on a line, then save the height data.
                    //    heightBuffer[((j / texels_per_height) * totalHeightmapSize) + (i / texels_per_height) + num_height_bin_args] = elevation_m;
                    //}

                    if (mat) {
                        if ((i % 20 == 0) && (j % 20 == 0)) {
                            sprintf(message, "Found a texel! mat %s, elev %f", mat->get_names()[0].c_str(), elevation_m);
                            SG_LOG(SG_GENERAL, SG_INFO, message);
                        }
                        
                        if (!strcmp(mat->get_names()[0].c_str(), "BuiltUpCover"))
                            textureBuffer[(j * totalHeightmapSize) + i] = (char)4;//totalTextureSize
                        else if (!strcmp(mat->get_names()[0].c_str(), "EvergreenBroadCover"))
                            textureBuffer[(j * totalHeightmapSize) + i] = (char)2;
                        else if (!strcmp(mat->get_names()[0].c_str(), "DryCropPastureCover"))
                            textureBuffer[(j * totalHeightmapSize) + i] = (char)3;
                        else if (!strcmp(mat->get_names()[0].c_str(), "Grass"))
                            textureBuffer[(j * totalHeightmapSize) + i] = (char)4;
                        else if (!strcmp(mat->get_names()[0].c_str(), "Sand"))
                            textureBuffer[(j * totalHeightmapSize) + i] = (char)5;
                        else if ((!strcmp(mat->get_names()[0].c_str(), "Lake")) ||
                                 (!strcmp(mat->get_names()[0].c_str(), "Pond")) ||
                                 (!strcmp(mat->get_names()[0].c_str(), "Reservoir")) ||
                                 (!strcmp(mat->get_names()[0].c_str(), "Stream")) ||
                                 (!strcmp(mat->get_names()[0].c_str(), "Canal")) ||
                                 (!strcmp(mat->get_names()[0].c_str(), "Lagoon")) ||
                                 (!strcmp(mat->get_names()[0].c_str(), "Estuary")) ||
                                 (!strcmp(mat->get_names()[0].c_str(), "Watercourse")) ||
                                 (!strcmp(mat->get_names()[0].c_str(), "Saline")))
                            textureBuffer[(j * totalHeightmapSize) + i] = (char)6;
                        else {
                            bool newUnID = true;
                            for (int k = 0; k < unidentified_textures.size(); k++) {
                                if (!strcmp(unidentified_textures[k].c_str(), mat->get_names()[0].c_str()))
                                    newUnID = false;
                            }
                            if (newUnID) {
                                unidentified_textures.insert(unidentified_textures.begin(), mat->get_names()[0]);
                                printf("Unidentified texture: %s\n", mat->get_names()[0].c_str());
                            }
                            textureBuffer[(j * totalHeightmapSize) + i] = (char)4;
                        }
                    } else {
                        //SG_LOG(SG_GENERAL, SG_INFO, "NO texel! elev %d", elevation_m);
                        textureBuffer[(j * totalHeightmapSize) + i] = (char)4;
                    }
                }
            }
        }    
    }

    if (true) //(texels_per_height==0)
    {
        for (int i = 0; i < totalHeightmapSize; i++) {
            for (int j = 0; j < totalHeightmapSize; j++) {
                elevation_m = 0.0f;
                kPos.setLatitudeDeg(startLat + (j * heightSizeLat));
                kPos.setLongitudeDeg(startLong + (i * heightSizeLong));
                kPos.setElevationM(0);
                if (kScenery->get_elevation_m(SGGeod::fromGeodM(kPos, 3000),
                                              elevation_m, &bvhMat, 0)) {
                    heightBuffer[(j * totalHeightmapSize) + i + num_height_bin_args] = elevation_m;
                    if ((i % 40 == 0) && (j % 40 == 0)) {
                        sprintf(message, "found elevation  %f", elevation_m);
                        SG_LOG(SG_GENERAL, SG_INFO, message);
                    }
                }
            }
        }
    }
    //printf("FOUND ALL THE MATERIALS! %s  total size %d\n",mat->get_names()[0].c_str(),totalTextureSize*totalTextureSize);

    fp = NULL;
    ////std::string textureBinfilePath = resourcePath + "terrain.textures.bin";
    ////sprintf(tempChars,"terrain_%d_%d.textures.bin",y,x);

    if (!(fp = fopen(textureBinfilePath.c_str(), "wb")))
        return;
    fwrite(textureBuffer, texture_array_size, 1, fp);
    fclose(fp);

    free(textureBuffer);

    //std::string heightBinfilePath = resourcePath + "terrain.heights.bin";
    //sprintf(tempChars,"terrain_%d_%d.heights.bin",y,x);
    printf("Writing to heights bin file: %s  start lat/long %f  %f \n",
           heightBinfilePath.c_str(), startLat, startLong);
    if (!(fp = fopen(heightBinfilePath.c_str(), "wb")))
        return;
    heightCharBuffer = reinterpret_cast<char*>(heightBuffer);
    fwrite(heightCharBuffer, height_array_size * sizeof(float), 1, fp);
    fclose(fp);


    sprintf(message, "worldDataSource - finished making terrain tile!!");
    SG_LOG(SG_GENERAL, SG_INFO, message);
}
