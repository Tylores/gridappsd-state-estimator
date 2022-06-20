#define DIAGONAL_P
#define SWITCHES
//#define NET_INJECTION
#ifdef NET_INJECTION
//#define COMPARE_INJ_MEAS
#endif

#define DEBUG_PRIMARY
//#define DEBUG_FILES
//#define DEBUG_SIZES

//#define SBASE_TESTING

#define GS_OPTIMIZE
// whether to store negligable values in sparse matrices or not
// defining USE_NEGL indicates not to store negligable values
//#define USE_NEGL

// files-based interface related conditional compilation values
#ifdef FILE_INTERFACE
//#define FILE_INTERFACE_READ "test_4Ti"
//#define FILE_INTERFACE_READ "test_3p6_Ti"
//#define FILE_INTERFACE_READ "test_4PiQi"
//#define FILE_INTERFACE_READ "test_13assets_noaji"
//#define FILE_INTERFACE_READ "test_11big_jl"
//#define FILE_INTERFACE_READ "test_4"
//#define FILE_INTERFACE_READ "test_4vinj"
//#define FILE_INTERFACE_READ "test_4net"
//#define FILE_INTERFACE_READ "test_4sbase"
//#define FILE_INTERFACE_READ "test_13assets"
//#define FILE_INTERFACE_READ "test_11full"
//#define FILE_INTERFACE_READ "test_11diff"
//#define FILE_INTERFACE_READ "test_11noQ"
//#define FILE_INTERFACE_READ "test_4withB"
//#define FILE_INTERFACE_READ "test_4woB"
//#define FILE_INTERFACE_READ "test_11big"
//#define FILE_INTERFACE_READ "test_3p6"
//#define FILE_INTERFACE_READ "test_3p6pseudo"
//#define FILE_INTERFACE_READ "test_11_bus_full"
//#define FILE_INTERFACE_READ "test_11_bus_diff"
//#define FILE_INTERFACE_READ "test_11_bus_full_meas"
//#define FILE_INTERFACE_READ "test_11_bus_diff_meas"
//#define FILE_INTERFACE_READ "test_4_bus_full"
//#define FILE_INTERFACE_READ "test_4_bus_diff"
//#define FILE_INTERFACE_READ "test_3p6_bus_full"
//#define FILE_INTERFACE_READ "test_3p6_bus_diff"
//#define FILE_INTERFACE_READ "test_3p6_bus_full_meas"
//#define FILE_INTERFACE_READ "test_3p6_bus_diff_meas"
#define FILE_INTERFACE_READ "test_files_123"
//#define FILE_INTERFACE_READ "test_files_13assets"

#ifndef FILE_INTERFACE_READ
#include "OOPS, NEED TO DEFINE FILE_INTERFACE_READ!"
#endif

// whether to get node_vnoms from file or hardwire to 1
#define FILE_INTERFACE_VNOM
// the nosbase symbol is used for a model outside GridAPPS-D like the
// 4-bus MATLAB model
//#define FILE_INTERFACE_NOSBASE
#endif

//#define FILE_INTERFACE_WRITE

#define PI 3.141592653589793

#include <iostream>
#include <fstream>
#include <regex>
  using std::regex;
#include <string>
  using std::string;
#include <complex>
  using std::complex;
#include <list>
#include <unordered_map>

#define SLIST std::list<std::string>
#define SIMAP std::unordered_map<std::string,uint>
#define SDMAP std::unordered_map<std::string,double>
#define SCMAP std::unordered_map<std::string,std::complex<double>>
#define SBMAP std::unordered_map<std::string,bool>
#define SSMAP std::unordered_map<std::string,std::string>
#define ISMAP std::unordered_map<uint,std::string>
#define IDMAP std::unordered_map<uint,double>
#define IMDMAP std::unordered_map<uint,IDMAP>
#define ICMAP std::unordered_map<uint,std::complex<double>>
#define IMMAP std::unordered_map<uint,ICMAP>
#define SSLISTMAP std::unordered_map<std::string,SLIST>

// global logging stream--either a file or stdout based on invocation
std::ostream* selog = &std::cout;

// include files for all interfaces
#include "SensorArray.hpp"

#ifdef GRIDAPPSD_INTERFACE
// include files for the GridAPPS-D interface
#include "json.hpp"
  using json = nlohmann::json;

#include "SEConsumer.hpp"
#include "GenericConsumer.hpp"
#include "TopoProcConsumer.hpp"
#include "VnomConsumer.hpp"
#include "SensorDefConsumer.hpp"
#include "SEProducer.hpp"
#include "state_estimator_gridappsd.hpp"
  using state_estimator_gridappsd::gridappsd_session;
#include "gridappsd_requests.hpp"
  using gridappsd_requests::sparql_query;
#include "sparql_queries_CIM100.hpp"
#include "state_estimator_util.hpp"
#include "SharedQueue.hpp"
#include "SELoopConsumer.hpp"

#ifdef DEBUG_PRIMARY
// temporary flag to hold up initialization until the platform has finished
// its own initialization for the simulation based on sending a STARTED message
bool blockedFlag = true;
#endif
#endif

// more include files for all interfaces
#include "SELoopWorker.hpp"

#include "PlatformInterfaceCommon.hpp"
#ifdef FILE_INTERFACE
#include "PlatformInterfaceFile.hpp"
#endif
#ifdef GRIDAPPSD_INTERFACE
//#include "PlatformInterfaceGridAPPSD.hpp"
#endif


int main(int argc, char** argv) {
    // Common/shared interface code

    // Node bus mRIDs and phases data structures
    SSMAP node_bmrids;
    SSMAP node_phs;

    // Topology data structures
    uint node_qty;      // number of nodes
    SLIST node_names;   // list of node names
    SIMAP node_idxs;    // map from node name to unit-indexed position
    ISMAP node_name_lookup;
    IMMAP Yphys;        // double map from indices to complex admittance
    // G, B, g, and b are derived from Yphys:
    //    -- Gij = std::real(Yphys[i][j]);
    //    -- Bij = std::imag(Yphys[i][j]);
    //    -- gij = std::real(-1.0*Yphys[i][j]);
    //    -- bij = std::imag(-1.0*Yphys[i][j]);

    // Node nominal voltages data structure
    SCMAP node_vnoms;

    // system base power, functionally arbitrary -- can be tweaked
    // for better numerical stability if needed
    // all values in the approximate range 1e-140 to 1e+150 converge
    // and only numeric overflow/underflow results in failures for the
    // 3 models tested (ieee13nodecktassets, ieee123, test9500new)
    // values in the 1e+6 to 1e+12 range result in minimum Supd condition
    // numbers with the range for lowest condition varying somewhat between
    // the 3 models tested
#ifdef SBASE_TESTING
    double spower = (double)std::stoi(argv[3]);
    const double sbase = pow(10.0, spower);
#else
#ifdef FILE_INTERFACE_NOSBASE
    const double sbase = 1;
#else
    const double sbase = 1.0e+6;
#endif
#endif

    // --------------------------------------------------------------------
    // SENSOR INITIALIZATION
    // --------------------------------------------------------------------

    // A matrix data structures
    IMDMAP Amat;
    SSMAP regid_primnode_map;
    SSMAP regid_regnode_map;

    // Sensors data structures
    SensorArray zary;
    SSMAP mmrid_pos_type_map;
    SSMAP switch_node1s;
    SSMAP switch_node2s;

#ifdef GRIDAPPSD_INTERFACE
    // ------------------------------------------------------------------------
    // INITIALIZE THE STATE ESTIMATOR SESSION WITH RUNTIME ARGS
    // ------------------------------------------------------------------------
    state_estimator_gridappsd::state_estimator_session se;
    if ( !se.init(argc,argv) ) return 0;

    // ------------------------------------------------------------------------
    // INITIALIZE THE GRIDAPPS-D SESSION
    // ------------------------------------------------------------------------
    state_estimator_gridappsd::gridappsd_session gad(se);

#ifdef DEBUG_PRIMARY
    // determine whether to write to a log file or stdout based on whether
    // this is a platform vs. command line invocation
    static std::ofstream logfile;
    if (gad.stateEstimatorFromPlatformFlag) {
        logfile.open("/tmp/state-estimator.log");
        selog = &logfile;
    }
#endif

    // declare the thread-safe queue shared between SELoopConsumer (writer)
    // and SELoopWorker (reader)
    SharedQueue<json> workQueue;

    // ------------------------------------------------------------------------
    // START THE AMQ INTERFACE
    // ------------------------------------------------------------------------
    activemq::library::ActiveMQCPP::initializeLibrary();

    // --------------------------------------------------------------------
    // LISTEN FOR SIMULATION LOG MESSAGES
    // --------------------------------------------------------------------
    // simulation status (running, complete) comes from log messages
    string simlogTopic = "goss.gridappsd.simulation.log."+gad.simid;

    SELoopConsumer simLogConsumer(&workQueue, gad.brokerURI, gad.username,
        gad.password, simlogTopic, "topic");
    Thread simLogConsumerThread(&simLogConsumer);
    simLogConsumerThread.start();    // execute simLogConsumer.run
    simLogConsumer.waitUntilReady(); // wait for the startup latch release
#ifdef DEBUG_PRIMARY
    *selog << "Listening for simulation log messages on "+simlogTopic+'\n' << std::flush;
#endif

    // --------------------------------------------------------------------
    // LISTEN FOR SIMULATION MEASUREMENTS
    // --------------------------------------------------------------------
    // measurements come from either simulation output or sensor-simulator
    string topic = gad.useSensorsForEstimatesFlag?
        "goss.gridappsd.simulation.gridappsd-sensor-simulator."+gad.simid+".output":
        "goss.gridappsd.simulation.output."+gad.simid;

    SELoopConsumer measurementConsumer(&workQueue, gad.brokerURI,
                            gad.username, gad.password, topic, "topic");
    Thread measurementConsumerThread(&measurementConsumer);
    measurementConsumerThread.start();    // execute measurementConsumer.run
    measurementConsumer.waitUntilReady(); // wait for the startup latch release
#ifdef DEBUG_PRIMARY
    if (gad.useSensorsForEstimatesFlag)
        *selog << "Listening for sensor-simulator output on "+topic+'\n' << std::flush;
    else
        *selog << "Listening for simulation output on "+topic+'\n' << std::flush;
#endif

#ifdef DEBUG_PRIMARY
    // only block initialization for command line invocations
    //if (false) {
    if (!gad.stateEstimatorFromPlatformFlag) {
        *selog << "\nWaiting for simulation to start before continuing with initialization\n" << std::flush;
        while (blockedFlag) sleep(1);
        *selog << "\nSimulation started--continuing with initialization\n" << std::flush;
    } else {
        *selog << "\nNOT waiting before continuing with initialization\n" << std::flush;
    }
#endif

    // --------------------------------------------------------------------
    // MAKE SOME SPARQL QUERIES
    // --------------------------------------------------------------------
    // get node bus mRIDs and phases needed to publish results
    state_estimator_util::get_nodes(gad,node_bmrids,node_phs);

    // --------------------------------------------------------------------
    // TOPOLOGY PROCESSOR
    // --------------------------------------------------------------------
    // Set up the producer shared by ybus, vnom, and sensor requests
    string requestTopic = "goss.gridappsd.process.request.config";
    SEProducer requester(gad.brokerURI,gad.username,gad.password,requestTopic,"queue");

    // Set up the ybus consumer
    string ybusTopic = "goss.gridappsd.se.response."+gad.simid+".ybus";
    TopoProcConsumer ybusConsumer(gad.brokerURI,gad.username,gad.password,ybusTopic,"queue");
    Thread ybusConsumerThread(&ybusConsumer);
    ybusConsumerThread.start();        // execute ybusConsumer.run()
    ybusConsumer.waitUntilReady();    // wait for latch release

    // Request ybus with previously created producer
    string ybusRequestText =
        "{\"configurationType\":\"YBus Export\",\"parameters\":{\"simulation_id\":\""
        + gad.simid + "\"}}";
    requester.send(ybusRequestText,ybusTopic);

    // Wait for topology processor and retrieve topology (ybus, node info)
    ybusConsumerThread.join();
    ybusConsumer.fillTopo(node_qty,node_names,node_idxs,node_name_lookup,Yphys);
    ybusConsumer.close();

    // Set up the vnom consumer
    string vnomTopic = "goss.gridappsd.se.response."+gad.simid+".vnom";
    VnomConsumer vnomConsumer(gad.brokerURI,gad.username,gad.password,vnomTopic,"queue");
    Thread vnomConsumerThread(&vnomConsumer);
    vnomConsumerThread.start();        // execute vnomConsumer.run()
    vnomConsumer.waitUntilReady();    // wait for latch release

    // Request vnom with previously created producer
    string vnomRequestText =
        "{\"configurationType\":\"Vnom Export\",\"parameters\":{\"simulation_id\":\""
        + gad.simid + "\"}}";
    requester.send(vnomRequestText,vnomTopic);

    // Wait for the vnom processor and retrive vnom
    vnomConsumerThread.join();
    vnomConsumer.fillVnom(node_vnoms);
    vnomConsumer.close();

    SSMAP reg_cemrid_primbus_map;
    SSMAP reg_cemrid_regbus_map;
    state_estimator_util::build_A_matrix(gad,Amat,node_idxs,
            reg_cemrid_primbus_map,reg_cemrid_regbus_map,
            regid_primnode_map,regid_regnode_map);

    // map conducting equipment to bus names
    SSLISTMAP cemrid_busnames_map;
    state_estimator_util::build_cemrid_busnames_map(gad, cemrid_busnames_map);

    // Adds nominal load injections
    SDMAP node_nominal_Pinj_map;
    SDMAP node_nominal_Qinj_map;
    state_estimator_util::get_nominal_energy_consumer_injections(gad,
            node_vnoms,node_nominal_Pinj_map,node_nominal_Qinj_map);

    // Set up the sensors consumer
    string sensTopic = "goss.gridappsd.se.response."+gad.simid+".cimdict";
    SensorDefConsumer sensConsumer(gad.brokerURI,gad.username,gad.password,
            cemrid_busnames_map,reg_cemrid_primbus_map,reg_cemrid_regbus_map,
            node_nominal_Pinj_map,node_nominal_Qinj_map,
            sbase,sensTopic,"queue");
    Thread sensConsumerThread(&sensConsumer);
    sensConsumerThread.start();       // execute sensConsumer.run()
    sensConsumer.waitUntilReady();    // wait for latch release

    // Request sensor data with previously created producer
    string sensRequestTopic = "goss.gridappsd.process.request.config";
    string sensRequestText = "{\"configurationType\":\"CIM Dictionary\",\"parameters\":{\"simulation_id\":\""
            + gad.simid + "\"}}";
    requester.send(sensRequestText,sensTopic);
    requester.close(); // this is the last request so close it off

    // Wait for sensor initializer and retrieve sensors
    sensConsumerThread.join();
    sensConsumer.fillSens(zary,mmrid_pos_type_map,switch_node1s,switch_node2s);
    sensConsumer.close();

    // Add Pseudo-Measurements
    state_estimator_util::insert_pseudo_measurements(gad,zary,
            node_names,node_vnoms,sbase);
#ifdef DEBUG_PRIMARY
    //*selog << "\nzsigs/zvals after adding pseudo-measurements:\n" << std::flush;
    //for ( auto& zid : zary.zids ) {
    //    *selog << "\tzid: " << zid << ", ztype: " << zary.ztypes[zid] << ", zsig: " << zary.zsigs[zid] << ", zvals: " << zary.zvals[zid] << "\n" << std::flush;
    //}
#endif

    // Initialize class that does the state estimates
    SELoopWorker loopWorker(&workQueue, &gad, zary, node_qty, node_names,
        node_idxs, node_vnoms, node_bmrids, node_phs, node_name_lookup,
        sbase, Yphys, Amat, regid_primnode_map, regid_regnode_map,
        mmrid_pos_type_map, switch_node1s, switch_node2s);
#endif

#ifdef FILE_INTERFACE
    PlatformInterface pi;

    pi.fillTopologyMinimal(Yphys, node_names);

    node_qty = 0;
    for ( auto& node_name : node_names ) {
        node_idxs[node_name] = ++node_qty;
        node_name_lookup[node_qty] = node_name;
    }

    pi.fillVnom(node_names, node_vnoms);

    pi.fillMeasurements(node_idxs, zary, Amat,
        regid_primnode_map, regid_regnode_map);

    // Initialize class that does the state estimates
    SELoopWorker loopWorker(zary, node_qty, node_names,
        node_idxs, node_vnoms, node_bmrids, node_phs, node_name_lookup,
        sbase, Yphys, Amat, regid_primnode_map, regid_regnode_map,
        mmrid_pos_type_map, switch_node1s, switch_node2s);
#endif

    // Common/shared interface code

#ifdef DEBUG_PRIMARY
    *selog << "Starting the SE work loop\n" << std::flush;
#endif
    loopWorker.workLoop();

    // we'll never get here
    return 0;
}
