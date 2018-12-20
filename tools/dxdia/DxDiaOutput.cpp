#include "DxDiaOutput.h"

#include "DxDiaResult.h"

namespace {
struct CloseHandleRAII {
  explicit CloseHandleRAII(HANDLE h) : handle(h) {}
  ~CloseHandleRAII() { CloseHandle(handle); }
  HANDLE handle = INVALID_HANDLE_VALUE;
};
}  // namespace

void dxdia::WriteToFile(
    IDxcBlob *Blob,
    llvm::StringRef filename) {
  llvm::StringRef contents((LPSTR)Blob->GetBufferPointer(),
                           Blob->GetBufferSize());

  //  Creates the new file to write the PDB contents.
  HANDLE hTempFile = CreateFileA(filename.data(),         // file name 
                                 GENERIC_WRITE,           // open for write 
                                 0,                       // do not share 
                                 NULL,                    // default security 
                                 CREATE_ALWAYS,           // overwrite existing
                                 FILE_ATTRIBUTE_NORMAL,   // normal file 
                                 NULL);                   // no template 
  if (hTempFile == INVALID_HANDLE_VALUE) {
    FATAL_ERROR(OutputFileCreationErr);
  }
  CloseHandleRAII ch(hTempFile);

  DWORD dwBytesWritten = 0;
  const bool fSuccess = WriteFile(hTempFile,
                                  contents.data(),
                                  contents.size(),
                                  &dwBytesWritten,
                                  NULL);
  if (!fSuccess || (dwBytesWritten != contents.size())) {
    FATAL_ERROR(OutputFileCreationErr);
  }
}

void dxdia::WritePDBToTmpFile(
    IDxcBlob *pdb,
    std::string *pdb_filename) {
  pdb_filename->clear();
  TCHAR szTempFileName[MAX_PATH];
  TCHAR lpTempPathBuffer[MAX_PATH];
  DWORD dwRetVal = GetTempPath(MAX_PATH,           // length of the buffer
                               lpTempPathBuffer);  // buffer for path
  if (dwRetVal > MAX_PATH || (dwRetVal == 0)) {
    FATAL_ERROR(TempFileCreationErr);
  }

  //  Generates a temporary file name. 
  UINT uRetVal = GetTempFileName(lpTempPathBuffer,  // directory for tmp files
                                 TEXT("dxdia-pdb"), // temp file name prefix 
                                 0,                 // create unique name 
                                 szTempFileName);   // buffer for name 
  if (uRetVal == 0) {
    FATAL_ERROR(TempFileCreationErr);
  }

  try {
    WriteToFile(pdb, szTempFileName);
  } catch (const dxdia::FatalError &f) {
    if (f.kind != dxdia::FatalError::Kind::OutputFileCreationErr) {
      throw;
    }
    FATAL_ERROR(TempFileCreationErr);
  }
  *pdb_filename = szTempFileName;
}
