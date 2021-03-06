
#include <algorithm>
#include <cassert>
#include <iostream>

#include "artdaq-demo/Compression/Decoder.hh"

using namespace std;
using namespace demo;

Decoder::Decoder(SymTable const & s): syms_(s), table_(), head_(syms_.size())
{
  sort(syms_.begin(), syms_.end(),
       [](SymCode const & a, SymCode const & b) 
       { return a.bit_count_ < b.bit_count_; });

  reverseCodes(syms_);
  table_.reserve(syms_.size() * 10);
  for_each(syms_.begin(), syms_.end(),
  [&](SymCode const & s) { table_.push_back(s.sym_); });
  last_ = table_.size();
  head_ = addNode();
  buildTable();
}

void Decoder::buildTable()
{
  for (size_t s = 0, len = syms_.size(); s < len; ++s) {
    // cout << "adding sym " << syms_[s] << endl;
    reg_type code = syms_[s].code_;
    size_t index = 0;
    size_t curr = head_; // start at top;
    // go through all bits in the code for the symbol
    for (size_t i = 0; i < syms_[s].bit_count_ - 1; ++i) {
      index = code & 0x01;
      if (table_[curr + index] == neg_one) {
        size_t newnode = addNode();
        table_[curr + index] = newnode;
        // cout << "adding new node " << newnode << " at " << (curr+index) << " last=" << last_ << "\n";
      }
      curr = table_[curr + index];
      code >>= 1;
    }
    index = code & 0x01;
    if (table_[curr + index] != neg_one) {
      std::cerr << "table[" << (curr) << "]=" << table_[curr] << " last=" << last_ << "\n";
      throw "bad run!";
    }
    // cout << "final = " << (curr+index) << " val=" << s << " sym=" << syms_[s] <<  "\n";
    table_[curr + index] = s;
  }
}

void Decoder::printTable(std::ostream & ost) const
{
  SymTable non_reversed_syms(syms_);
  reverseCodes(non_reversed_syms);
  for (auto c = table_.cbegin() + head_, e = table_.cend(); c != e; ++c) {
    if (*c < head_) { ost << "L" << *c << " sym=" << non_reversed_syms[*c] << "\n"; }
    else { ost << "B" << *c << "\n"; }
  }
  ost << "table size=" << table_.size() << " last=" << last_ << endl;
}

reg_type Decoder::operator()(reg_type bit_count,
                             DataVec::const_iterator in,
                             adc_type * out_ptr,
                             adc_type const * out_end __attribute__((unused)),
			     bool use_diffs)
{
  adc_type * const start = out_ptr; // Save start position.
  size_t curr = head_;
  reg_type const * pos = &*in;
  reg_type val = *pos;
  adc_type prev=0;

  for (reg_type i = 0; i < bit_count; ++i) 
    {
      auto inc = (i % 64 + 1) / 64;
      // cout << i << " ";
      curr = table_[curr + (val & 0x01)];
      val >>= 1;
      if (inc) 
	{
	  // cout << "inc" << endl;
	  ++pos;
	  val = *pos;
	}
      if (curr < head_)
	{
	  assert(out_ptr != out_end); // Safety check.
	  if(use_diffs)
	    {
	      adc_type x = (adc_type)syms_[curr].sym_ + prev;
	      *out_ptr++ = x;
	      prev = x;
	    }
	  else
	    {
	      adc_type adcval = (adc_type)syms_[curr].sym_;
	      *out_ptr++ = adcval;
	    }
	  curr = head_;
	}
    }
  return out_ptr - start; // Result;
}

reg_type Decoder::operator()(reg_type bit_count, DataVec const & in, ADCCountVec & out)
{
  ADCCountVec tmp;
  size_t curr = head_;
  reg_type const * pos = &in[0];
  reg_type val = *pos;
  for (reg_type i = 0; i < bit_count; ++i) {
    auto inc = (i % 64 + 1) / 64;
    // cout << i << " ";
    curr = table_[curr + (val & 0x01)];
    val >>= 1;
    if (inc) {
      // cout << "inc" << endl;
      ++pos;
      val = *pos;
    }
    if (curr < head_) {
      adc_type adcval = (adc_type)syms_[curr].sym_;
      tmp.push_back(adcval);
      curr = head_;
    }
  }
  tmp.swap(out);
  return out.size();
}
