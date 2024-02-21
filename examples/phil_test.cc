/*
Copyright (c) 2019-2026, Hossein Moein
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Hossein Moein and/or the DataFrame nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Hossein Moein BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <DataFrame/DataFrame.h>                   // Main DataFrame header
#include <DataFrame/DataFrameFinancialVisitors.h>  // Financial algorithms
#include <DataFrame/DataFrameMLVisitors.h>         // Machine-learning algorithms
#include <DataFrame/DataFrameStatsVisitors.h>      // Statistical algorithms
#include <DataFrame/Utils/DateTime.h>              // Cool and handy date-time object

#include <iostream>

// -----------------------------------------------------------------------------

// DataFrame library is entirely under hmdf name-space
//
using namespace hmdf;

// A DataFrame with ulong index type
//
using ULDataFrame = StdDataFrame<unsigned long>;

// A DataFrame with string index type
//
using StrDataFrame = StdDataFrame<std::string>;

// A DataFrame with DateTime index type
//
using DTDataFrame = StdDataFrame<DateTime>;

// This is just some arbitrary type to show how any type could be in DataFrame
//
struct  MyData  {
    int         i { 10 };
    double      d { 5.5 };
    std::string s { "Some Arbitrary String" };

    MyData() = default;
};

// -----------------------------------------------------------------------------

// The main purpose of this file is to introduce the basic operations of DataFrame.
// For more advanced operations and a complete list of features with code samples, see documentation at:
// https://htmlpreview.github.io/?https://github.com/hosseinmoein/DataFrame/blob/master/docs/HTML/DataFrame.html
//
int main(int, char *[])  {
  std::cout << "data/IBM.csv : io_format::csv2" << std::endl;
  StrDataFrame    ibm_master;
  ibm_master.read("data/IBM.csv", io_format::csv2);

  auto  above_150_fun = [](const std::string &, const double &val)-> bool { return (val > 150.0); };
  auto  above_150_df = ibm_master.get_data_by_sel<double, decltype(above_150_fun), double, long>("IBM_Close", above_150_fun);

  auto above_150_view =
      ibm_master.get_view_by_sel<double, decltype(above_150_fun), double, long>("IBM_Close", above_150_fun);

  std::cout << "There are " << above_150_df.get_index().size() << " above_150_df indices" << std::endl;

  // No way to insert this.
  std::string candiateRow = "2024-02-21,98.000000,98.000000,92.250000,93.500000,59.98,10651400";

  std::string index_val = "2024-02-21";

  above_150_df.append_row(&index_val,
                //std::make_pair("INDEX", std::string("2024-02-21")),
                std::make_pair("IBM_Open", 12),
                std::make_pair("IBM_High", 12),
                std::make_pair("IBM_Low", 12),
                std::make_pair("IBM_Close", 12),
                std::make_pair("IBM_Adj_Close", 12),
                std::make_pair("IBM_Volume", 12));

  above_150_df.write<std::ostream, std::string, double, double, double, double, double, long>(std::cout, io_format::csv2);

  return (0);
}

// -----------------------------------------------------------------------------

// Local Variables:
// mode:C++
// tab-width:4
// c-basic-offset:4
// End:
