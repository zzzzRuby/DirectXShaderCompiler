#pragma once

#include "dxc/Support/WinIncludes.h"

#include <string>

#include "dxc/dxcapi.h"
#include "llvm/ADT/StringRef.h"

namespace dxdia {
void WritePDBToTmpFile(
    IDxcBlob *pdb,
    std::string *pdb_filename);

void WriteToFile(
    IDxcBlob *Blob,
    llvm::StringRef filename);
}  // namespace dxdia

