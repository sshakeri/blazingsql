#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>


#include "io/DataLoader.h"
#include "io/data_parser/CSVParser.h"
#include "io/data_parser/DataParser.h"
#include "io/data_parser/ParquetParser.h"
#include "io/data_provider/DataProvider.h"
#include "io/data_provider/UriDataProvider.h"
#include <fstream>
#include <gdf_wrapper/gdf_wrapper.cuh>

#include <arrow/io/file.h>
#include <arrow/util/logging.h>

#include "execution_graph/logic_controllers/LogicalFilter.h"
#include "utilities/DebuggingUtils.h"
 

#ifndef PARQUET_FILE_PATH
#error PARQUET_FILE_PATH must be defined for precompiling
#define PARQUET_FILE_PATH "/"
#endif

namespace cudf_io = cudf::experimental::io;
using blazingdb::manager::experimental::Context;
using Node = blazingdb::transport::experimental::Node;


struct MinMaxParquetTest : public ::testing::Test {

  void SetUp() { ASSERT_EQ(rmmInitialize(nullptr), RMM_SUCCESS); }

  void TearDown() { ASSERT_EQ(rmmFinalize(), RMM_SUCCESS); }
};


TEST_F(MinMaxParquetTest, UsingRalIO) {

  std::string filename = PARQUET_FILE_PATH;
	std::cout << "filename: " << filename << std::endl;
	
	std::vector<Uri> uris;
	uris.push_back(Uri{filename});
	ral::io::Schema schema;
	auto parser = std::make_shared<ral::io::parquet_parser>();
	auto provider = std::make_shared<ral::io::uri_data_provider>(uris);
 
	{
		cudf_io::read_parquet_args in_args{cudf_io::source_info{PARQUET_FILE_PATH}};
		auto result = cudf_io::read_parquet(in_args);
		for(auto name : result.metadata.column_names) {
			std::cout << "col_name: " << name << std::endl;
		}
		std::cerr << " <<<  reading parquet:: " <<   std::endl;
		// expect_tables_equal(expected->view(), result.tbl->view());
	}
	std::cerr << ">>> reading metadata:: " <<   std::endl;
 
	ral::io::data_loader loader(parser, provider);
	try {
		auto table_pair = loader.get_metadata(0);
		auto view = table_pair->toBlazingTableView();
		// ral::utilities::print_blazing_table_view(view);
	} catch(std::exception e){
		std::cerr << "***std::exception:: " <<  e.what() << std::endl;

	}
}
