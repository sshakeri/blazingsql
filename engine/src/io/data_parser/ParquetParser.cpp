
#include "metadata/parquet_metadata.h"

#include "ParquetParser.h"
#include "config/GPUManager.cuh"
#include <blazingdb/io/Util/StringUtil.h>
#include <cudf/legacy/column.hpp>
#include <cudf/legacy/io_functions.hpp>


#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <algorithm>
#include <numeric>
#include <string>
#include <cudf/cudf.h>
#include <cudf/io/functions.hpp>

#include <unordered_map>
#include <vector>
#include <arrow/io/file.h>
#include <parquet/file_reader.h>
#include <parquet/schema.h>
#include <parquet/types.h>
#include <thread>

#include <parquet/column_writer.h>
#include <parquet/file_writer.h>
#include <parquet/properties.h>
#include <parquet/schema.h>
#include <parquet/types.h>

#include <parquet/api/reader.h>
#include <parquet/api/writer.h>

#include "../Schema.h"

#include "gdf_wrapper.cuh"

#include <numeric>

namespace ral {
namespace io {

namespace cudf_io = cudf::experimental::io;

parquet_parser::parquet_parser() {
	// TODO Auto-generated constructor stub
}

parquet_parser::~parquet_parser() {
	// TODO Auto-generated destructor stub
}

ral::frame::TableViewPair parquet_parser::parse(
	std::shared_ptr<arrow::io::RandomAccessFile> file,
	const std::string & user_readable_file_handle,
	const Schema & schema,
	std::vector<size_t> column_indices) 
{
	if(column_indices.size() == 0) {  // including all columns by default
		column_indices.resize(schema.get_num_columns());
		std::iota(column_indices.begin(), column_indices.end(), 0);
	}

	if(file == nullptr) {
		return std::make_pair(nullptr, ral::frame::BlazingTableView());
	}

	if(column_indices.size() > 0) {
		// Fill data to pq_args
		// cudf::io::parquet::reader_options pq_args;
		cudf_io::read_parquet_args pq_args{cudf_io::source_info{file}};

		pq_args.strings_to_categorical = false;
		pq_args.columns.resize(column_indices.size());

		for(size_t column_i = 0; column_i < column_indices.size(); column_i++) {
			pq_args.columns[column_i] = schema.get_name(column_indices[column_i]);
		}
		// TODO: Use schema.row_groups_ids to read only some row_groups
		// cudf::io::parquet::reader parquet_reader(file, pq_args);
		// cudf::table table_out = parquet_reader.read_all();

		auto result = cudf_io::read_parquet(pq_args);


		// columns_out.resize(column_indices.size());

		// for(size_t i = 0; i < columns_out.size(); i++) {
		// if(table_out.get_column(i)->dtype == GDF_STRING) {
		// NVStrings * strs = static_cast<NVStrings *>(table_out.get_column(i)->data);
		// NVCategory * category = NVCategory::create_from_strings(*strs);
		// std::string column_name(table_out.get_column(i)->col_name);
		// columns_out[i].create_gdf_column(category, table_out.get_column(i)->size, column_name);
		// gdf_column_free(table_out.get_column(i));
		// } else {
		// TODO percy cudf0.12 port cudf::column and io stuff
		//columns_out[i].create_gdf_column(table_out.get_column(i));
		// }
		// }
		std::unique_ptr<ral::frame::BlazingTable> table_out = std::make_unique<ral::frame::BlazingTable>(std::move(result.tbl), result.metadata.column_names);
		ral::frame::BlazingTableView table_out_view = table_out->toBlazingTableView();
		return std::make_pair(std::move(table_out), table_out_view);
	}
	return std::make_pair(nullptr, ral::frame::BlazingTableView());
}


void parquet_parser::parse_schema(
	std::vector<std::shared_ptr<arrow::io::RandomAccessFile>> files, ral::io::Schema & schema_out) {

	std::vector<size_t> num_row_groups(files.size());
	std::thread threads[files.size()];
	for(int file_index = 0; file_index < files.size(); file_index++) {
		threads[file_index] = std::thread([&, file_index]() {
			std::unique_ptr<parquet::ParquetFileReader> parquet_reader =
				parquet::ParquetFileReader::Open(files[file_index]);
			std::shared_ptr<parquet::FileMetaData> file_metadata = parquet_reader->metadata();
			const parquet::SchemaDescriptor * schema = file_metadata->schema();
			num_row_groups[file_index] = file_metadata->num_row_groups();
			parquet_reader->Close();
		});
	}

	for(int file_index = 0; file_index < files.size(); file_index++) {
		threads[file_index].join();
	}

	cudf::io::parquet::reader_options pq_args;
	pq_args.strings_to_categorical = false;
	cudf::io::parquet::reader cudf_parquet_reader(files[0], pq_args);
	cudf::table table_out = cudf_parquet_reader.read_rows(0, 1);


	// we currently dont support GDF_DATE32 for parquet so lets filter those out
	std::vector<std::string> column_names_out;
	std::vector<cudf::type_id> dtypes_out;
	for(size_t i = 0; i < table_out.num_columns(); i++) {
		if(table_out.get_column(i)->dtype != GDF_DATE32) {
			column_names_out.push_back(table_out.get_column(i)->col_name);
			dtypes_out.push_back(to_type_id(table_out.get_column(i)->dtype));
		}
	}
	table_out.destroy();

	std::vector<std::size_t> column_indices(column_names_out.size());
	std::iota(column_indices.begin(), column_indices.end(), 0);

	schema_out = ral::io::Schema(column_names_out, column_indices, dtypes_out, num_row_groups);
}


std::unique_ptr<ral::frame::BlazingTable> parquet_parser::get_metadata(std::vector<std::shared_ptr<arrow::io::RandomAccessFile>> files, int offset){
	std::vector<size_t> num_row_groups(files.size());
	std::thread threads[files.size()];
	std::vector<std::unique_ptr<parquet::ParquetFileReader>> parquet_readers(files.size());
	for(int file_index = 0; file_index < files.size(); file_index++) {
		threads[file_index] = std::thread([&, file_index]() {
		  parquet_readers[file_index] =
			  parquet::ParquetFileReader::Open(files[file_index]);
		  std::shared_ptr<parquet::FileMetaData> file_metadata = parquet_readers[file_index]->metadata();
		  const parquet::SchemaDescriptor * schema = file_metadata->schema();
		  num_row_groups[file_index] = file_metadata->num_row_groups();
		});
	}

	for(int file_index = 0; file_index < files.size(); file_index++) {
		threads[file_index].join();
	}

	size_t total_num_row_groups =
		std::accumulate(num_row_groups.begin(), num_row_groups.end(), size_t(0));

	auto minmax_metadata_table = get_minmax_metadata(parquet_readers, total_num_row_groups, offset);
	for (auto &reader : parquet_readers) {
		reader->Close();
	}
	return std::move(minmax_metadata_table);
}

} /* namespace io */
} /* namespace ral */
