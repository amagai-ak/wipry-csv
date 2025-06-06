/**
 * @file main.cpp
 * @brief 
 * @version 0.1
 * @date 2025-05-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "WiPryClarity.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <signal.h>
#include <cstring>

#include <getopt.h>

// 	original source: https://github.com/bryanward-net/wipry-lp
/*
    wipry-lp

    Outputs Oscium WiPry spectrum analysis data in Influx Line Protocol format

    Copyright (c) 2023 Matt Lee and Bryan Ward

*/

// modified by amagai


std::string VERSION = "v0.1.0";


using namespace oscium;
WiPryClarity* wipryClarity = nullptr;

static volatile bool run = true;
unsigned int band;
std::string serial;

bool isConnected = false; // flag denoting that there is a successful connection
bool connectionProcessComplete = true; // flag denoting that the connection process has completed
unsigned int rssi2_4GHzFrameCount;
unsigned int rssi5GHzFrameCount;
unsigned int rssi6EFrameCount;


float even_min, even_max, even_noisefloor;
float odd_min, odd_max, odd_noisefloor;

float freqLow2, freqHigh2;
float freqLow5, freqHigh5;
float freqLow6, freqHigh6;


// This the delegate class that receives the events from the WiPryClarity object.
class MyDelegate : public WiPryClarityDelegate {
protected:
	int rcvCount = 0;
	int rcvLimit = 0;
	bool csvmode = false;

public:
	void setRcvLimit(int n) {
		rcvLimit = n;
	}

	void setCsvMode(bool mode) {
		csvmode = mode;
	}

	void wipryClarityDidConnect(WiPryClarity *aWipryClarity) {
		std::cerr << "Connected to device." << std::endl;
		isConnected = true;
		connectionProcessComplete = true;
		rcvCount = 0;

		float min, max, noisefloor;
		float freqLow, freqHigh;
		bool result;

		result = aWipryClarity->getEvenRssiLimts(&min, &max, &noisefloor);
		if (result) {
			std::cerr << "Even Min:" << min << "dBm Max:" << max << "dBm Noise Floor:" << noisefloor << "dBm" << std::endl;
			even_min = min;
			even_max = max;
			even_noisefloor = noisefloor;
		}
		else
			std::cerr << "Unable to get limits" << std::endl;

		result = aWipryClarity->getOddRssiLimts(&min, &max, &noisefloor);
		if (result) {
			std::cerr << " Odd Min:" << min << "dBm Max:" << max << "dBm Noise Floor:" << noisefloor << "dBm" << std::endl;
			odd_min = min;
			odd_max = max;
			odd_noisefloor = noisefloor;
		}
		else
			std::cerr << "Unable to get limits" << std::endl;

		result = aWipryClarity->get2_4GHzBoundary(&freqLow, &freqHigh);
		if (result) {
			std::cerr << "2.4GHz Low:" << freqLow << "MHz High:" << freqHigh << "MHz" << std::endl;
			freqLow2 = freqLow;
			freqHigh2 = freqHigh;
		}
		else
			std::cerr << "Unable to get 2.4GHz frequency limits" << std::endl;

		result = aWipryClarity->get5GHzBoundary(&freqLow, &freqHigh);
		if (result) {
			std::cerr << "  5GHz Low:" << freqLow << "MHz High:" << freqHigh << "MHz" << std::endl;
			freqLow5 = freqLow;
			freqHigh5 = freqHigh;
		}
		else
			std::cerr << "Unable to get 5GHz frequency limits" << std::endl;

		result = aWipryClarity->get6EBoundary(&freqLow, &freqHigh);
		if (result) {
			std::cerr << "    6E Low:" << freqLow << "MHz High:" << freqHigh << "MHz" << std::endl;
			freqLow6 = freqLow;
			freqHigh6 = freqHigh;
		}
		else
			std::cerr << "Unable to get 6E frequency limits" << std::endl;

		rssi2_4GHzFrameCount = 0;
		rssi5GHzFrameCount = 0;
		rssi6EFrameCount = 0;
	}

	void wipryClarityUnableToConnect(WiPryClarity *wipryClarity, WiPryClarity::ErrorCode errorCode) {
		std::cerr << "Disconnected from Accessory with Error Code : " << (int)errorCode << std::endl;
		// delete the global WiPry Clarity object
		if (wipryClarity != nullptr)
		{
			if (wipryClarity->didStartCommunication())
				wipryClarity->endCommunication();

			// NOTE: Do not delete the wipry2500 object here or it will cause a crash. Delete it somewhere else.
			// delete wipryClarity;
			// wipryClarity = nullptr;

			connectionProcessComplete = true;
			isConnected = false;
		}
	}

	//Not Implemented in library
	/*
	void wipryClarityDidReceiveBeaconCaptureData(WiPryClarity *wipryClarity, std::vector<uint8_t> beaconCaptures) {
		if (beaconCaptures.size() > 0) {
			std::cerr<<"Got " << beaconCaptures.size() << " beacons!" << std::endl;
		}
	}
	*/


	// RSSIをInfluxDB Line Protocol形式で出力する
	void dumpRSSI_lp(WiPryClarity::DataType dataType, std::vector<float> rssiData, float freqLow, float freqHigh, long long timens) {
		float stepsize = ( (freqHigh - freqLow) / (int)rssiData.size() );

		std::cout << "wipry,serial=" << serial;
		switch (dataType)
		{
			case oscium::WiPryClarity::DataType::RSSI_2_4GHZ:
				std::cout << ",band=2 ";
				break;
			case oscium::WiPryClarity::DataType::RSSI_5GHZ:
				std::cout << ",band=5 ";
				break;
			case oscium::WiPryClarity::DataType::RSSI_6E:
				std::cout << ",band=6 ";
				break;
			default:
				break;
		}

		for (int p=0; p < (int)rssiData.size(); p++) {
			if (p < ((int)rssiData.size() - 1))
				std::cout << (freqLow + p * stepsize) << "=" <<(int)rssiData[p] << ",";
			else
				std::cout << (freqLow + p * stepsize) << "=" <<(int)rssiData[p];
		}
		std::cout << " " << timens << std::endl;
	}

	// RSSIをCSV形式で出力する
	// CSVは，count, timestamp, band, freq_low, freq_high, step, points, rssi1, rssi2, ...
	void dumpRSSI_csv(WiPryClarity::DataType dataType, std::vector<float> rssiData, float freqLow, float freqHigh, long long timens) {
		float stepsize = ( (freqHigh - freqLow) / (int)rssiData.size() );
		double time_s = (double)timens / 1000000000.0;
		std::cout << rcvCount << ",";
		// time_sは，小数点以下3桁まで表示
		std::cout << std::fixed << std::setprecision(3) << time_s << ",";
		switch (dataType)
		{
			case oscium::WiPryClarity::DataType::RSSI_2_4GHZ:
				std::cout << "2,";
				break;
			case oscium::WiPryClarity::DataType::RSSI_5GHZ:
				std::cout << "5,";
				break;
			case oscium::WiPryClarity::DataType::RSSI_6E:
				std::cout << "6,";
				break;
			default:
				break;
		}
		std::cout << freqLow << ",";
		std::cout << freqHigh << ",";
		std::cout << stepsize << ",";
		std::cout << (int)rssiData.size() << ",";
		for (int p=0; p < (int)rssiData.size(); p++) {
			if (p < ((int)rssiData.size() - 1))
				std::cout << (int)rssiData[p] << ",";
			else
				std::cout << (int)rssiData[p];
		}
		std::cout << std::endl;
	}

	// データ受信時に呼ばれる処理
	void wipryClarityDidReceiveRSSIData(WiPryClarity *aWipryClarity, WiPryClarity::DataType dataType, std::vector<float> rssiData) {
		long long timens = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
		float freqLow, freqHigh;
		if( run == false ) {
			// std::cerr << "Received data after stop command. Ignoring." << std::endl;
			return;
		}

		switch(dataType)
		{
			case oscium::WiPryClarity::DataType::RSSI_2_4GHZ:
				freqLow = freqLow2;
				freqHigh = freqHigh2;
				rssi2_4GHzFrameCount++;
				break;
			case oscium::WiPryClarity::DataType::RSSI_5GHZ:
				freqLow = freqLow5;
				freqHigh = freqHigh5;
				rssi5GHzFrameCount++;
				break;
			case oscium::WiPryClarity::DataType::RSSI_6E:
				freqLow = freqLow6;
				freqHigh = freqHigh6;
				rssi6EFrameCount++;
				break;
			default:
				std::cerr << "Unknown data type received." << std::endl;
				return;
		}

		if( csvmode ){
			dumpRSSI_csv(dataType, rssiData, freqLow, freqHigh, timens);
		} else {
			dumpRSSI_lp(dataType, rssiData, freqLow, freqHigh, timens);
		}
		rcvCount++;
		if( rcvLimit > 0 && rcvCount >= rcvLimit ) {
			std::cerr << "Received " << rcvCount << " frames. Stopping data stream." << std::endl;
			run = false;
		}
	}
};


static void sig_handler (int param)
{
  run = false;
}


void helptext() {
        std::cout << "Outputs Oscium WiPry spectrum analysis data in Influx Line Protocol format" << std::endl;
        std::cout << std::endl;
		std::cout << "Usage:" << std::endl;
		std::cout << std::endl;
        std::cout << "    wipry-lp -[2|5|6] [-n N]" << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
		std::cout << "	-2		Run on the 2.4GHz Band" << std::endl;
		std::cout << "	-5		Run on the 5GHz Band" << std::endl;
		std::cout << "	-6		Run on the 6GHz Band" << std::endl;
        //Not yet implemented.  Supported by libWiPryClarity, but requires special handling of non-contiguous spectrum data
	//std::cout << "	-D		Run on both the 2.4GHz and 5GHz Band" << std::endl;
        //Not yet implemented.  ToDo: Requires logic to manually switch between bands while running
	//std::cout << "	-T		Run on all three bands" << std::endl;
		std::cout << "	-c		CSV output instead of line protocol." << std::endl;
		std::cout << "	-h		Print this help text and exit." << std::endl;
		std::cout << "	-n	N	Stop after N scans." << std::endl;

        std::cout << std::endl;
        std::cout << std::endl;
        std::cout << "Originally built by Bryan Ward, based on sample code graciously provided by Matt Lee from Oscium." << std::endl;
        std::cout << "" << std::endl;
        std::cout << "wipry-csv Version " << VERSION << std::endl;
        //std::cout << "Build Date " << __BUILDTIMESTAMP__ << std::endl;
        std::cout << "libWiPryClarity version " << oscium::WiPryClarity::getVersion() << std::endl;
        std::cout << "Copyright (c) 2023 Matt Lee and Bryan Ward" << std::endl;
}


MyDelegate delegate;

int main(int argc, char *argv[]) {

	int c;
	int ntimes;
	bool csvmode = false;

	if (argc <= 1) {
		std::cerr << "No band specified!" << std::endl;
		helptext();
		return 1;
	}

	// getopt
	ntimes = 0;
	while((c = getopt(argc, argv, "ch256n:")) != -1) {
		switch(c) {
			case 'h':
				helptext();
				return 0;
			case '2':
				band = 2;
				break;
			case '5':
				band = 5;
				break;
			case '6':
				band = 6;
				break;
			case 'c':
				csvmode = true;
				break;
			case 'n':
				ntimes = atoi(optarg);
				if (ntimes < 0) {
					std::cerr << "Invalid number of times specified!" << std::endl;
					helptext();
					return 1;
				}
				break;
			default:
				std::cerr << "Invalid Argument Specified!" << std::endl;
				helptext();
				return 1;
		}
	}

	wipryClarity = new WiPryClarity();
	wipryClarity->setDelegate(&delegate);
	delegate.setCsvMode(csvmode);

	// start the connection
	std::cerr<< "Starting connection process." << std::endl;
	connectionProcessComplete = false;
	isConnected =false;

	bool result = wipryClarity->startCommunication();
	if (result == false) {
		std::cerr<< "Error: Unable to connect to the WiPryClarity. Make sure that the device has been connected." << std::endl;
		delete wipryClarity;
		wipryClarity = nullptr;
		return 1;
	}

	// wait for the connection prcoess to complete
	while(!connectionProcessComplete) {
		std::chrono::milliseconds duration(100);
		std::this_thread::sleep_for(duration);
	}

	if ( isConnected )
	{
		std::cerr<< "Connection Success." << std::endl;
		serial = wipryClarity->getSerialNumber();
		std::cerr<< "Serial Number: " << serial << std::endl;
	} else {
		// connection failed
		delete wipryClarity;
		return 1;
	}

	[[maybe_unused]] void (*sigint_handler)(int);
	sigint_handler = signal(SIGINT, sig_handler);
	[[maybe_unused]] void (*sigterm_handler)(int);
	sigterm_handler = signal(SIGTERM, sig_handler);
	[[maybe_unused]] void (*sigabrt_handler)(int);
	sigabrt_handler = signal(SIGABRT, sig_handler);


	delegate.setRcvLimit(ntimes); // set to 0 for infinite

	if (band == 2) {
		// start 2.4 Ghz Rssi data
		std::cerr << "Starting 2.4 GHz rssi data stream." << std::endl;
		wipryClarity->startRssiData(oscium::WiPryClarity::DataType::RSSI_2_4GHZ, 0, 0, 0);
	}

	if (band == 5) {
		// start 5 Ghz Rssi data
		std::cerr << "Starting 5 GHz rssi data stream." << std::endl;
		wipryClarity->startRssiData(oscium::WiPryClarity::DataType::RSSI_5GHZ, 0, 0, 0);
	}

	if (band == 6) {
		// start 6 GHz Rssi data
		std::cerr << "Starting 6 GHz rssi data stream." << std::endl;
		wipryClarity->startRssiData(oscium::WiPryClarity::DataType::RSSI_6E, 0, 0, 0);
	}

	if (band == 25) {
		// start Dual band RSSI data
		std::cerr << "Starting Dual Band rssi data stream." << std::endl;
		wipryClarity->startRssiData(oscium::WiPryClarity::DataType::RSSI_DUAL25, 0, 0, 0);
	}

	while (run) {
	    std::this_thread::yield();
		//Prevent CPU spinlock
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
       }

	// stop the data
	std::cerr << "Stopping rssi data stream." << std::endl;
	wipryClarity->stopRssiData();

	// sleep for 500ms
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	// closing connection
	std::cerr<< "Closing connection to WiPry Clarity." << std::endl;
	if (wipryClarity->didStartCommunication())
		wipryClarity->endCommunication();
	delete wipryClarity;
	wipryClarity = nullptr;

	return 0;
}
