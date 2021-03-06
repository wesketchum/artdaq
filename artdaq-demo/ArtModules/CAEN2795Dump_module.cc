////////////////////////////////////////////////////////////////////////
// Class:       CAEN2795Dump
// Module Type: analyzer
// File:        CAEN2795Dump_module.cc
// Description: Prints out information about each event.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "art/Utilities/Exception.h"
#include "artdaq-core-demo/Overlays/CAEN2795Fragment.hh"
#include "artdaq-core/Data/Fragments.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>

namespace demo {
  class CAEN2795Dump;
}

class demo::CAEN2795Dump : public art::EDAnalyzer {
public:
  explicit CAEN2795Dump(fhicl::ParameterSet const & pset);
  virtual ~CAEN2795Dump();

  virtual void analyze(art::Event const & evt);

private:
  std::string raw_data_label_;
  uint32_t num_adcs_to_show_;
};


demo::CAEN2795Dump::CAEN2795Dump(fhicl::ParameterSet const & pset)
    : EDAnalyzer(pset),
      raw_data_label_(pset.get<std::string>("raw_data_label")),
      num_adcs_to_show_(pset.get<uint32_t>("num_adcs_to_show", 0))
		      
{
}

demo::CAEN2795Dump::~CAEN2795Dump()
{
}

void demo::CAEN2795Dump::analyze(art::Event const & evt)
{
  art::EventNumber_t eventNumber = evt.event();

  // ***********************
  // *** CAEN2795 Fragments ***
  // ***********************

  // look for raw CAEN2795 data

  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, raw);

  if(!raw.isValid()){
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
              << ", event " << eventNumber << " has zero"
              << " CAEN2795 fragments." << std::endl;
    std::cout << std::endl;
    return;
  }


  std::cout << "######################################################################" << std::endl;
  std::cout << std::endl;
  std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	    << ", event " << eventNumber << " has " << raw->size()
	    << " CAEN2795 fragment(s)." << std::endl;
  
  for (size_t idx = 0; idx < raw->size(); ++idx) {
    const auto& frag((*raw)[idx]);
    
    CAEN2795Fragment bb(frag);
    
    std::cout << std::endl;
    std::cout << "CAEN2795 fragment " << frag.fragmentID() << " has total ADC counts = " 
	      << bb.total_adc_values() << std::endl;
    std::cout << std::endl;
    
    if (frag.hasMetadata()) {
      std::cout << std::endl;
      std::cout << "Fragment metadata: " << std::endl;
      CAEN2795Fragment::Metadata const* md =
	frag.metadata<CAEN2795Fragment::Metadata>();
      std::cout << "\tChannels per board: " << md->channels_per_board
		<< "\n\tNumber of boards: " << md->num_boards
		<< "\n\tNumber of ADC Bits: " << md->num_adc_bits
		<< std::endl;
      std::cout << std::endl;
    }

    if (num_adcs_to_show_ == 0) {
      num_adcs_to_show_ = bb.total_adc_values();
    }
    
    if (num_adcs_to_show_ > 0) {
      
      if (num_adcs_to_show_ > bb.total_adc_values() ) {
	throw cet::exception("num_adcs_to_show is larger than total number of adcs in fragment");
      } else {
	std::cout << std::endl;
	std::cout << "First " << num_adcs_to_show_ 
		  << " ADC values in the fragment: " 
		  << std::endl;
      }
      
      std::ofstream output ("out.bin", std::ios::out | std::ios::app | std::ios::binary );
      for (uint32_t i_adc = 0; i_adc < num_adcs_to_show_; ++i_adc) {
	output.write((char*)(bb.dataBegin() + i_adc),sizeof(CAEN2795Fragment::adc_t));
	std::cout << std::hex << std::setfill('0') << std::setw(4) << *(bb.dataBegin() + i_adc);
	if(i_adc%8==7) std::cout << std::endl;
      }
      output.close();
      std::cout << std::dec << std::endl;
      std::cout << std::endl;
    }
  }
}

DEFINE_ART_MODULE(demo::CAEN2795Dump)
