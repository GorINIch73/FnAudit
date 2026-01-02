#pragma once

#include <string>
#include <vector>
#include <map>
#include "DatabaseManager.h"

// Represents the mapping from a target field name (e.g., "Дата") 
// to the index of the column in the source file.
using ColumnMapping = std::map<std::string, int>;

class ImportManager {
public:
    ImportManager();

    // Imports payments from a TSV file using a user-defined column mapping.
    bool ImportPaymentsFromTsv(const std::string& filepath, DatabaseManager* dbManager, const ColumnMapping& mapping);

};
