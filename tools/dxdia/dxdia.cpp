#include "dxc/Support/WinIncludes.h"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <comdef.h>

#include "dia2.h"
#include "d3dcompiler.h"

#include "dxc/dxcapi.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/Unicode.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"

#include "DxDiaInput.h"
#include "DxDiaOutput.h"
#include "DxDiaResult.h"
#include "DxDiaSymTag.h"

HRESULT CreateDxcDiaDataSource(_In_ REFIID riid, _Out_ LPVOID* ppv);

namespace cl = llvm::cl;

static cl::opt<std::string> DiaQuery(
  cl::Positional,
  cl::desc("<dia query>"),
  cl::init(""),
  cl::Required);

static cl::opt<std::string> InputFilename(
  cl::Positional,
  cl::desc("<input bitcode>"),
  cl::init("-"));

static cl::opt<std::string> D3DCompiler(
  "with-d3dcompiler",
  cl::desc("Specifies the path to the d3dcompiler dll to be used if --use-d3dcompiler"),
  cl::init("d3dcompiler_47.dll"));

static cl::opt<std::string> DXCompiler(
  "with-dxcompiler",
  cl::desc("Specifies the path to the dxcompiler dll to be used if the input is not bitcode"),
  cl::init("dxcompiler.dll"));

static cl::opt<bool> UseD3DCompiler(
  "use-d3dcompiler",
  cl::desc("Use d3dcompiler (the one specified with --with-d3dcompiler) for creating the Dia data source"),
  cl::init(false));

static cl::opt<std::string> Entrypoint(
  "E",
  cl::desc("Specifies the entry point (when compiling HLSL)"),
  cl::init("main"));

static cl::opt<std::string> TargetProfile(
  "T",
  cl::desc("Specifies the target profile (when compiling HLSL)"),
  cl::init("cs_5_0"));

static cl::opt<dxdia::InputKind> InputFormat(
    cl::desc("Input format"),
    cl::values(
      clEnumValN(dxdia::InputKind::LLVM, "llvm", "Input is an LLVM module - binary or ll"),
      clEnumValN(dxdia::InputKind::Blob, "blob", "Input is a DXBC/DXIL Blob"),
      clEnumValN(dxdia::InputKind::HLSL, "hlsl", "Input is an HLSL source"),
      clEnumValN(dxdia::InputKind::PDB, "pdb", "Input is a PDB"),
      clEnumValEnd));

static void PrintSymbol(IDiaSymbol *P, IDiaSymbol *S, std::wstring ident, std::wstring prefix);

std::vector<CComPtr<IDiaSymbol>> knownSyms;
static void FindChildrenAndPrint(IDiaSymbol *P, IDiaEnumSymbols *Syms, std::wstring ident, std::wstring prefix) {
  //if (Syms->Reset() != S_OK) {
  //  FATAL_ERROR(ChildrenEnumErr);
  //}

  static constexpr ULONG kNumElts = 1;
  ULONG Count;
  bool done = false;
  uint32_t sCount = 1;
  for (CComPtr<IDiaSymbol> S; !done; S.Release(), ++sCount) {
    switch (Syms->Next(kNumElts, &S, &Count))
    {
    case S_FALSE:
      done = true;
      // fallthrough
    case S_OK:
      break;
    default:
      FATAL_ERROR(ChildrenEnumErr);
    }

    if (Count == kNumElts) {
      //knownSyms.emplace_back(S);
      PrintSymbol(P, S, ident + L"  ", prefix + L"." + std::to_wstring(sCount));
      std::wcout << "\n" << std::flush;
    }
  }
}

static void PrintSymbol(IDiaSymbol *P, IDiaSymbol *S, std::wstring ident, std::wstring prefix) {
  CComPtr<IDiaSymbol> actualP;
  switch (S->get_lexicalParent(&actualP)) {
  default:
    FATAL_ERROR(ChildrenEnumErr);
  case S_OK:
  case S_FALSE:
    break;
  }
  //if (actualP != P) {
  //  return;
  //}

  BSTR str = {};
  switch (S->get_name(&str)) {
  default:
    FATAL_ERROR(ChildrenEnumErr);
  case S_OK:
  case S_FALSE:
    break;
  }

  if (str == nullptr) {
    str = L"<null>";
  }

  DWORD ST;
  std::wstring sST = L"<none>";
  switch (S->get_symTag(&ST)) {
  default:
    FATAL_ERROR(ChildrenEnumErr);
  case S_OK:
    sST = dxdia::SymTagToStr((decltype(SymTagNull))ST);
  case S_FALSE:
    break;
  }
  std::wcout << ident << prefix << ": " << (void*) S << " " << (void*) actualP.p << " " << str << " " << sST;

  VARIANT v = {};
  bool hasValue = false;
  switch (S->get_value(&v)) {
  case S_OK:
    hasValue = true;
  case S_FALSE:
    break;
  default:
    FATAL_ERROR(ChildrenEnumErr);
  }

  if (hasValue) {
    switch (v.vt) {
    case 0:
      std::wcout << "([vt] 0)";
      break;
    case VT_BSTR:
      std::wcout << ": " << v.bstrVal;
      break;
    case VT_I4:
      std::wcout << ": " << v.intVal;
      break;
    case VT_UI4:
      std::wcout << ": " << v.uintVal;
      break;
    default:
      std::wcout << ": ??? ([vt]" << v.vt << ")";
    }
  }
  CComPtr<IDiaEnumSymbols> Syms;
  switch (S->findChildren(SymTagNull, nullptr, nsNone, &Syms)) {
  default:
    FATAL_ERROR(ChildrenEnumErr);
  case E_NOTIMPL:
  case S_FALSE:
    break;
  case S_OK:
    std::wcout << "\n";
    FindChildrenAndPrint(S, Syms, ident + L"  ", prefix);
    return;
  }
}

static void PrintTable(IDiaTable *T, std::wstring ident, std::wstring prefix) {
  BSTR str;
  if (T->get_name(&str) != S_OK) {
    FATAL_ERROR(ChildrenEnumErr);
  }
  std::wcout << ident << prefix << ": " << _bstr_t(str) << "\n" << std::flush;

  CComQIPtr<IDiaEnumSymbols> Syms = T;
  if (!Syms) {
    return;
  }

  FindChildrenAndPrint(nullptr, Syms, ident, prefix);
}

static void ProcessDiaDataSource(CComPtr<IDiaDataSource> DDS) {
  CComPtr<IDiaSession> DS;
  if (DDS->openSession(&DS) != S_OK) {
    FATAL_ERROR(SessionCreationErr);
  }

  llvm::SmallVector<llvm::StringRef, 16> tokens;
  static constexpr char Delimiter[] = ".";
  llvm::SplitString(DiaQuery, tokens, Delimiter);

  if (tokens.empty()) {
    std::cout << "no query provided - exiting..." << std::flush;
  }

  if (tokens[0] == "tables") {
    if (tokens.size() > 1) {
      FATAL_ERROR(QueryErr);
    }

    CComPtr<IDiaEnumTables> Tables;
    if (DS->getEnumTables(&Tables) != S_OK) {
      FATAL_ERROR(ChildrenEnumErr);
    }

    ULONG Count;
    static constexpr ULONG kReadCnt = 1;
    std::uint32_t tCount = 1;
    bool done = false;
    for (CComPtr<IDiaTable> Table; !done; Table.Release(), ++tCount) {
      switch (Tables->Next(kReadCnt, &Table, &Count)) {
      case S_FALSE:
        done = true;
        // fallthrough intended.
      case S_OK:
        break;
      default:
        FATAL_ERROR(ChildrenEnumErr);
      }

      if (Count == kReadCnt) {
        PrintTable(Table.p, L"", std::to_wstring(tCount));
      }
    }
  } else if (tokens[0] == "@") {
    if (tokens.size() > 1) {
      FATAL_ERROR(QueryErr);
    }

    CComPtr<IDiaSymbol> GS;
    if (DS->get_globalScope(&GS) != S_OK) {
      FATAL_ERROR(ChildrenEnumErr);
    }

    PrintSymbol(nullptr, GS, L"", L"@");
  }
}

namespace {
struct FreeModuleRAII {
  FreeModuleRAII() {}
  ~FreeModuleRAII() { if (handle) { FreeModule(handle); } }
  HMODULE handle = nullptr;
};
}  // namespace

static void DxDia(int argc, const char *argv[]) {
  if (llvm::sys::fs::SetupPerThreadFileSystem())
    FATAL_ERROR(ThreadFSCreationErr);
  llvm::sys::fs::AutoCleanupPerThreadFileSystem auto_cleanup_fs;
  if (FAILED(DxcInitThreadMalloc())) FATAL_ERROR(ThreadFSCreationErr);
  DxcSetThreadMallocOrDefault(nullptr);
  llvm::sys::fs::MSFileSystem* msfPtr;
  if (FAILED(CreateMSFileSystemForDisk(&msfPtr))) FATAL_ERROR(ThreadFSCreationErr);
  std::unique_ptr<llvm::sys::fs::MSFileSystem> msf(msfPtr);
  llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
  CoInitialize(nullptr);

  cl::ParseCommandLineOptions(argc, argv, "dxdia - a (standalone) tool for inspecting DXIL debug info");

  FreeModuleRAII hD3DCompilerRAII;
  auto d3dcompiler = [&hD3DCompilerRAII]() -> HMODULE {
    if (hD3DCompilerRAII.handle == nullptr) {
      hD3DCompilerRAII.handle = LoadLibrary(D3DCompiler.c_str());
      if (hD3DCompilerRAII.handle == nullptr) {
        FATAL_ERROR(CompilerLoadErr);
      }
    }
    return hD3DCompilerRAII.handle;
  };

  std::unique_ptr<dxc::DxcDllSupport> dxcompilerRAII;
  auto dxcompiler = [&dxcompilerRAII]() -> dxc::DxcDllSupport & {
    if (!dxcompilerRAII) {
      dxcompilerRAII.reset(new dxc::DxcDllSupport());
      if (dxcompilerRAII->InitializeForDll(_bstr_t(DXCompiler.c_str()), "DxcCreateInstance") != S_OK) {
        FATAL_ERROR(CompilerLoadErr);
      }
    }
    return *dxcompilerRAII;
  };

  // Buffer with the input data.
  CComPtr<dxdia::DxDiaBuffer> InputBuffer;
  std::string pdb_filename;
  switch (InputFormat)
  {
  default:
    FATAL_ERROR(FailedToLoadInput);
  case dxdia::InputKind::PDB:
    // PDB inputs are left alone, and msdia will read the file for us.
    // TODO: figure out why IDiaDataSource::loadFromIStream fails to load
    //       in memory PDBs.
    pdb_filename = InputFilename;
    break;
  case dxdia::InputKind::Blob:
  case dxdia::InputKind::HLSL:
  case dxdia::InputKind::LLVM:
    auto input_contents = llvm::MemoryBuffer::getFileOrSTDIN(InputFilename);
    if (!input_contents) {
      FATAL_ERROR(FailedToLoadInput);
    }
    InputBuffer = dxdia::DxDiaBuffer::Create(input_contents.get()->getBuffer());
    break;
  }

  // Buffer with a DXBC/DXIL blob.
  CComPtr<dxdia::DxDiaBuffer> BlobBuffer;
  switch (InputFormat)
  {
  default:
    FATAL_ERROR(FailedToLoadInput);
  case dxdia::InputKind::PDB:
  case dxdia::InputKind::LLVM:
    break;
  case dxdia::InputKind::Blob: {
    // The input data is a blob.
    BlobBuffer = InputBuffer;
    break;
  }
  case dxdia::InputKind::HLSL: {
    // The Compiler Output Blob.
    CComPtr<IDxcBlob> COB;

    if (UseD3DCompiler) {
      dxdia::ParseHLSLWithD3DCompiler(d3dcompiler(), InputBuffer, InputFilename, Entrypoint, TargetProfile, &COB);
    } else {
      dxdia::ParseHLSLWithDXCompiler(dxcompiler(), InputBuffer, InputFilename, Entrypoint, TargetProfile, &COB);
    }

    BlobBuffer = dxdia::DxDiaBuffer::Create(COB);
    break;
  }
  }

  // The Dxil buffer, if BlobBuffer is a DXIL blob.
  CComPtr<IDxcBlob> DxilBuffer;
  switch (InputFormat)
  {
  default:
    FATAL_ERROR(FailedToLoadInput);
  case dxdia::InputKind::PDB:
    assert(!BlobBuffer);
    break;
  case dxdia::InputKind::LLVM: {
    assert(!BlobBuffer);
    InputBuffer.QueryInterface(&DxilBuffer);
    break;
  }
  case dxdia::InputKind::Blob:
  case dxdia::InputKind::HLSL: {
    // The PDB buffer, if BlobBuffer is a DXBC blob.
    CComPtr<IDxcBlob> PdbBuffer;
    dxdia::ExtractDxilAndPDBBlobParts(dxcompiler(), BlobBuffer, &DxilBuffer, &PdbBuffer);
    if (DxilBuffer) {
      assert(!PdbBuffer && "DXIL blob should not have a PDB part.");
      // If the blob has a Dxil part, ignore any PDBs it may have.
      PdbBuffer.Release();
    }
    if (PdbBuffer) {
      // Create a temporary file housing the PDB so we can loadFromPDB in dxdia::LoadPDB.
      dxdia::WritePDBToTmpFile(PdbBuffer, &pdb_filename);
    }
    break;
  }
  }

  llvm::LLVMContext Ctx;
  llvm::SMDiagnostic Err;
  CComPtr<IDiaDataSource> DDS;
  if (!pdb_filename.empty()) {
    assert(!DxilBuffer && "pdb filename should be empty for DXIL blobs.");
    dxdia::LoadPBD(pdb_filename, &DDS);
  } else if (DxilBuffer) {
    llvm::StringRef DxilModuleRef((LPSTR)DxilBuffer->GetBufferPointer(),
                                  DxilBuffer->GetBufferSize());
    dxdia::LoadLLVMModule(DxilModuleRef, "loaded-dxil", &Ctx, &Err, &DDS);
  } else {
    FATAL_ERROR(FailedToLoadInput);
  }

  ProcessDiaDataSource(DDS);
}

int main(int argc, const char *argv[]) {
  int status = 0;
  try {
    DxDia(argc, argv);
  } catch (const dxdia::FatalError &err) {
    std::cerr
      << "Fatal dxdia error reported on " << err.filename << ':' << err.lineno << ": " << err.kind_str() << "\n";
    status = static_cast<std::uint32_t>(err.kind);
  }

  return status;
}