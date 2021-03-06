/*
 * Schema.cpp
 *
 *  Created on: Apr 26, 2019
 *      Author: felipe
 */

#include "Schema.h"

namespace ral {
namespace io {


std::string convert_dtype_to_string(const cudf::type_id & dtype) {
	if(dtype == cudf::type_id::STRING)
		return "str";
	// TODO percy cudf0.12 by default timestamp for bz is MS but we need to use proper time resolution
	if(dtype == cudf::type_id::TIMESTAMP_SECONDS)
		return "date64";
	if(dtype == cudf::type_id::TIMESTAMP_SECONDS)
		return "date32";
	// TODO percy cudf0.12 by default timestamp for bz is MS but we need to use proper time resolution
	if(dtype == cudf::type_id::TIMESTAMP_MILLISECONDS)
		return "timestamp";
	if(dtype == cudf::type_id::FLOAT32)
		return "float32";
	if(dtype == cudf::type_id::FLOAT64)
		return "float64";
	if(dtype == cudf::type_id::INT16)
		return "short";
	if(dtype == cudf::type_id::INT32)
		return "int32";
	if(dtype == cudf::type_id::INT64)
		return "int64";
	if(dtype == cudf::type_id::BOOL8)
		return "bool";

	return "str";
}

Schema::Schema(std::vector<std::string> names,
	std::vector<size_t> calcite_to_file_indices,
	std::vector<cudf::type_id> types,
	std::vector<size_t> num_row_groups,
	std::vector<std::vector<int>> row_groups_ids)
	: names(names), calcite_to_file_indices(calcite_to_file_indices), types(types), 
	  num_row_groups(num_row_groups), row_groups_ids{row_groups_ids} {
	// TODO Auto-generated constructor stub

	in_file.resize(names.size(), true);
}

Schema::Schema(std::vector<std::string> names,
	std::vector<size_t> calcite_to_file_indices,
	std::vector<cudf::type_id> types,
	std::vector<size_t> num_row_groups,
	std::vector<bool> in_file,
	std::vector<std::vector<int>> row_groups_ids)
	: names(names), calcite_to_file_indices(calcite_to_file_indices), types(types), num_row_groups(num_row_groups),
	  in_file(in_file), row_groups_ids{row_groups_ids}  {
	if(in_file.size() != names.size()) {
		this->in_file.resize(names.size(), true);
	}
}

Schema::Schema(std::vector<std::string> names, std::vector<cudf::type_id> types)
	: names(names), calcite_to_file_indices({}), types(types), num_row_groups({}) {
	in_file.resize(names.size(), true);
}

Schema::Schema() : names({}), calcite_to_file_indices({}), types({}), num_row_groups({}) {}


Schema::~Schema() {
	// TODO Auto-generated destructor stub
}

std::vector<std::string> Schema::get_names() const { return this->names; }

std::vector<std::string> Schema::get_types() const {
	std::vector<std::string> string_types;
	for(int i = 0; i < this->types.size(); i++) {
		string_types.push_back(convert_dtype_to_string(this->types[i]));
	}
	return string_types;
}

std::vector<std::string> Schema::get_files() const { return this->files; }

std::vector<cudf::type_id> Schema::get_dtypes() const { return this->types; }

std::string Schema::get_name(size_t schema_index) const { return this->names[schema_index]; }

std::string Schema::get_type(size_t schema_index) const { return convert_dtype_to_string(this->types[schema_index]); }

size_t Schema::get_file_index(size_t schema_index) const {
	if(this->calcite_to_file_indices.size() == 0) {
		return schema_index;
	}
	return this->calcite_to_file_indices[schema_index];
}

size_t Schema::get_num_row_groups(size_t file_index) const { return this->num_row_groups[file_index]; }

size_t Schema::get_num_columns() const { return this->names.size(); }

std::vector<bool> Schema::get_in_file() const { return this->in_file; }
/*void Schema::add_column(gdf_column_cpp column, size_t file_index) {
	this->names.push_back(column.name());
	this->types.push_back(column.get_gdf_column()->type().id());
	this->calcite_to_file_indices.push_back(file_index);
	this->in_file.push_back(true);
}*/

void Schema::add_column(std::string name, cudf::type_id type, size_t file_index, bool is_in_file) {
	this->names.push_back(name);
	this->types.push_back(type);
	this->calcite_to_file_indices.push_back(file_index);
	this->in_file.push_back(is_in_file);
}

void Schema::add_file(std::string file){
	this->files.push_back(file);
}

Schema Schema::fileSchema(size_t current_file_index) const {
	Schema schema;
	// std::cout<<"in_file size "<<this->in_file.size()<<std::endl;
	for(int i = 0; i < this->names.size(); i++) {
		size_t file_index = this->calcite_to_file_indices.size() == 0 ? i : this->calcite_to_file_indices[i];
		if(this->in_file[i]) {
			schema.add_column(this->names[i], this->types[i], file_index);
		}
	}
	// Just get the associated row_groups for current_file_index
	// TODO, @alex
	// schema.row_groups_ids.push_back(this->row_groups_ids.at(current_file_index));
	return schema;
}

} /* namespace io */
} /* namespace ral */
