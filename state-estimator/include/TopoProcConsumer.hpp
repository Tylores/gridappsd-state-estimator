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

#define ISMAP std::unordered_map<unsigned int,std::string>

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
    ISMAP node_name_lookup;
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
	void fillTopo(uint& numns, SLIST& nodens, SIMAP& nodem, 
            ISMAP& node_name_lookup, IMMAP& Y) {
		numns = this->numns;
		nodens = this->nodens;
		nodem = this->nodem;
        node_name_lookup = this->node_name_lookup;
		Y = this->Y;
	}
	
	public:
	virtual void process() {
		
		// --------------------------------------------------------------------
		// PARSE THE MESSAGE AND PROCESS THE TOPOLOGY
		// --------------------------------------------------------------------
#ifdef DEBUG_PRIMARY
		cout << "Received ybus message of " << text.length() << " bytes...\n\n" << std::flush;
#endif

		json jtext = json::parse(text);

#ifdef DEBUG_PRIMARY
        cout << "Parsing ybus ... " << std::flush;
#endif
		// This is actually a list of lines from ysparse
		json jlines_ysparse = jtext["data"]["yParse"];
		bool firstline = true;
		for ( auto& jline : jlines_ysparse ) {
			if (firstline) firstline = false;
			else {
				string tmpline = jline;
				// split the line {Row,Col,G,B}
				size_t pos = tmpline.find(",");
				int i = stoi( tmpline.substr(0,pos) );
					tmpline.erase(0,pos+1); pos = tmpline.find(",");
				int j = stoi( tmpline.substr(0,pos) );
					tmpline.erase(0,pos+1); pos = tmpline.find(",");
				double G = stod( tmpline.substr(0,pos) );
					tmpline.erase(0,pos+1); pos = tmpline.find(",");
				double B = stod( tmpline.substr(0,pos) );

				Y[i][j] = complex<double>(G,B);
				if ( i != j ) Y[j][i] = complex<double>(G,B);

			}
		}
#ifdef DEBUG_PRIMARY
        cout << "complete.\n\n" << std::flush;
        cout << "Parsing nodelist ... " << std::flush;
#endif
		// This is actually the list of nodes from nodelist
		json jlines_nodelist = jtext["data"]["nodeList"];
		int idx = 0;
		for ( auto& jline : jlines_nodelist ) {
			// Extract the node name
			string line = jline;
			string node_name = regex_replace(line,regex("\""),"");
			// Store the node information
			numns++;
			nodens.push_back(node_name);
			nodem[node_name] = ++idx;
            node_name_lookup[idx] = node_name;
		}
#ifdef DEBUG_PRIMARY
        cout << "complete.\n\n" << std::flush;
#endif
//		// print
//		for ( auto& inode : nodens ) {
//			auto i = nodem[inode];
//			try {
//				auto row = Y.at(i);
//				for ( auto& jnode: nodens ) {
//					auto j = nodem[jnode];
//					try {
//						complex<double> ycomp = row.at(j);
//						cout << "Y(" << i << "," << j << ") -> " << ycomp << "\n" << std::flush;
//					} catch(...) {}
//				}
//			} catch(...) {}
//		}

		// --------------------------------------------------------------------
		// TOPOLOGY PROCESSING COMPLETE
		// --------------------------------------------------------------------
		// release latch
		doneLatch.countDown();
	}
};

#endif