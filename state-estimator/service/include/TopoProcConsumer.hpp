#ifndef TOPOPROCCONSUMER_HPP
#define TOPOPROCCONSUMER_HPP

#include "json.hpp"
using json = nlohmann::json;

#include "SEConsumer.hpp"

// for side-loading test cases
#include <fstream>

// standard data types
#include <string>
#include <complex>
#include <list>
#include <unordered_map>

// macro for unsigned int
#define uint unsigned int

// Store node names in a linked list and hash node name to their position
// Iterate over the linked list to access all nodes or states
// Note that positions are one-indexed
// Store sensor names in a linked list and hash node names to various params
#define SLIST std::list<std::string>
#define SIMAP std::unordered_map<std::string,unsigned int>

// Hash address (i,j) to the index of a sparse matrix vector
#define ICMAP std::unordered_map<unsigned int,std::complex<double>>
#define IMMAP std::unordered_map<unsigned int,ICMAP>

using namespace std;

// This class listens for the Y-bus message and constructs the topology
class TopoProcConsumer : public SEConsumer {
	
	// Need to figure out how to get structures out of here:
	// Pointers or set reference
	private:
	SLIST nodens;
	SIMAP nodem;
	uint numns = 0;
	// to add a node:
	//	-- nodens.push_back(noden);
	//	-- nodem[ndoen] = ++numns;
	
	private:
	IMMAP Y;
	//	-- two-dimensional sparse matrix
	// To add an element:
	//	-- Y[i][j] = std::complex<double>(G,B);
//	// G, B, g, and b are derived from Y:
//	//	-- Gij = std::real(Y[i][j]);
//	//	-- Bij = std::imag(Y[i][j]);
//	//	-- gij = std::real(-1.0*Y[i][j]);
//	//	-- bij = std::imag(-1.0*Y[i][j]);
	
	public:
	TopoProcConsumer(const string& brokerURI, 
				const string& username,
				const string& password,
				const string& target,
				const string& mode) {
		this->brokerURI = brokerURI;
		this->username = username;
		this->password = password;
		this->target = target;
		this->mode = mode;
	}

	public:
	void fillTopo(uint& numns, SLIST& nodens, SIMAP& nodem, IMMAP& Y) {
		numns = this->numns;
		nodens = this->nodens;
		nodem = this->nodem;
		Y = this->Y;
	}
	
	public:
	virtual void process(const string& text) {
		
		// --------------------------------------------------------------------
		// PARSE THE MESSAGE AND PROCESS THE TOPOLOGY
		// --------------------------------------------------------------------
		cout << "\nRecieved ybus message: \n\t" + text + "\n\n";
		
		json jtext = json::parse(text);

		string yfn = jtext["data"]["yParseFilePath"];
		string nfn = jtext["data"]["nodeListFilePath"];

		cout << "yfn: " <<  yfn << "\n";
		cout << "nfn: " << nfn << "\n";
		
		// --------------------------------------------------------------------
		// LOAD THE LIST OF NODE NAMES
		// --------------------------------------------------------------------
		// For now, pull this in from a file:
		//	base_nodelist.csv from dss cmd "export ynodelist base_nodelist.csv"
		cout << "\nLoading node list from: " << nfn << '\n';
		std::ifstream nfs;
		nfs.open(nfn,std::ifstream::in);
		if ( !nfs ) {
			cout << "failed to open node file\n";
			throw "failed to open node name file";
		}
		std::string nfsl;
		while ( std::getline(nfs,nfsl) ) {
			// strip leading and trailing quotations and white space
			std::string noden = nfsl.substr(nfsl.find_first_not_of("\'\""),
					nfsl.find_last_not_of(" \t\f\v\n\r\'\""));
		//	std::cout << "|" << noden << "|\n";
			nodens.push_back(noden);
			nodem[noden] = ++numns;
		}
		nfs.close();
		// END pull node list from file
		
		cout << "\tNode list loaded.\n";
		
		// --------------------------------------------------------------------
		// LOAD THE Y-BUS MATRIX
		// --------------------------------------------------------------------
		// For now, pull the ybus from a file:
		// base_ysparse.csv from dss cmd "export y triplet base_ysparse.csv"
		cout << "\nLoading Y-bus from: " << yfn << '\n';
		std::ifstream yfs;
		yfs.open(yfn,std::ifstream::in);
		if ( !yfs ) {
			cout << "failed to open Y-bus file\n";
			throw "failed to open ybus file";
		}
		std::string yfsl;
		std::getline(yfs,yfsl);		// skip the header
		//std::cout<<yfsl<<'\n';		// print back the header
		int i,j;
		double G,B;
		char c;
		while ( yfs >> i >> c >> j >> c >> G >> c >> B ) {
//			cout << i << '\t' << j << '\t' << G << '\t' << B << '\n';
			Y[i][j] = std::complex<double>(G,B);
			if ( i != j ) Y[j][i] = std::complex<double>(G,B);
		}
		yfs.close();
		// END pull ybus from file
		
		cout << "\tY-bus loaded.\n";
		
		// --------------------------------------------------------------------
		// TOPOLOGY PROCESSING COMPLETE
		// --------------------------------------------------------------------
		// release latch
		doneLatch.countDown();
	}
};

#endif
