#include "DxDiaInput.h"

#include <comutil.h>
#include <d3dcompiler.h>

#include "dxc/dxcapi.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/microcom.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_os_ostream.h"

#include "DxDiaBuffer.h"
#include "DxDiaOutput.h"
#include "DxDiaResult.h"

void dxdia::ParseHLSLWithDXCompiler(
    dxc::DxcDllSupport &dxcompiler,
    IDxcBlob *InputBlob,
    llvm::StringRef input_filename,
    llvm::StringRef entrypoint,
    llvm::StringRef target_profile,
    IDxcBlob **ppBlob) {
  *ppBlob = nullptr;

  CComPtr<IDxcCompiler> c;
  if (dxcompiler.CreateInstance(CLSID_DxcCompiler, &c) != S_OK) {
    FATAL_ERROR(CompilerLoadErr);
  }

  if (input_filename == "-") {
    input_filename = "<stdin>";
  }

  CComPtr<IDxcOperationResult> res;
  LPCWSTR DxcArgs[] = {L"/Zi", L"/Od"};
  if (c->Compile(InputBlob,
                 _bstr_t(input_filename.data()),
                 _bstr_t(entrypoint.data()),
                 _bstr_t(target_profile.data()),
                  DxcArgs,
                  2,
                  nullptr,
                  0,
                  nullptr,
                  &res) != S_OK) {
    FATAL_ERROR(HLSLCompilationFailure);
  }

  HRESULT hr;
  if (res->GetStatus(&hr) != S_OK || hr != S_OK) {
    CComPtr<IDxcBlobEncoding> enc;
    if (res->GetErrorBuffer(&enc) == S_OK) {
      llvm::errs() << (LPSTR)enc->GetBufferPointer() << "\n";
    }
    
    FATAL_ERROR(HLSLCompilationFailure);
  }

  if (res->GetResult(ppBlob) != S_OK) {
    FATAL_ERROR(HLSLCompilationFailure);
  }
}

void dxdia::ParseHLSLWithD3DCompiler(
    HMODULE d3dcompiler_dll,
    IDxcBlob *InputBlob,
    llvm::StringRef input_filename,
    llvm::StringRef entrypoint,
    llvm::StringRef target_profile,
    IDxcBlob **ppBlob) {
  *ppBlob = nullptr;

  if (d3dcompiler_dll == nullptr) {
    FATAL_ERROR(CompilerLoadErr);
  }
  auto *c = reinterpret_cast<decltype(D3DCompile2) *>(GetProcAddress(d3dcompiler_dll, "D3DCompile2"));
  if (!c) {
    FATAL_ERROR(CompilerLoadErr);
  }

  CComPtr<ID3DBlob> Code;
  CComPtr<ID3DBlob> ErrorMsgs;
  if ((*c)(InputBlob->GetBufferPointer(),
           InputBlob->GetBufferSize(),
           input_filename.data(),
           /*defines=*/nullptr,
           D3D_COMPILE_STANDARD_FILE_INCLUDE,
           entrypoint.data(),
           target_profile.data(),
           D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
           0,
           0,
           nullptr,
           0,
           &Code,
           &ErrorMsgs) != S_OK) {
    FATAL_ERROR(HLSLCompilationFailure);
  }

  *ppBlob = DxDiaBuffer::Create(Code).Detach();
}

void dxdia::ExtractDxilAndPDBBlobParts(
    dxc::DxcDllSupport &dxcompiler,
    llvm::StringRef blob_str,
    IDxcBlob **ppDxil,
    IDxcBlob **ppPdb) {
  ExtractDxilAndPDBBlobParts(dxcompiler, DxDiaBuffer::Create(blob_str), ppDxil, ppPdb);
}

void dxdia::ExtractDxilAndPDBBlobParts(
    dxc::DxcDllSupport &dxcompiler,
    IDxcBlob *pBlob,
    IDxcBlob **ppDxil,
    IDxcBlob **ppPdb) {
  CComPtr<IDxcContainerReflection> c;
  if (dxcompiler.CreateInstance(CLSID_DxcContainerReflection, &c) != S_OK) {
    FATAL_ERROR(CompilerLoadErr);
  }

  if (c->Load(pBlob) != S_OK) {
    FATAL_ERROR(FailedToLoadInput);
  }

  std::uint32_t PC;
  if (c->GetPartCount(&PC) != S_OK) {
    FATAL_ERROR(FailedToLoadInput);
  }

  CComPtr<IDxcBlob> pDxil;
  CComPtr<IDxcBlob> pPdb;

  for (std::uint32_t i = 0; i < PC; ++i) {
    CComPtr<IDxcBlob> tmp;

    std::uint32_t DFCC;
    if (c->GetPartKind(i, &DFCC) != S_OK) {
      FATAL_ERROR(FailedToLoadInput);
    }

    if (DFCC == hlsl::DFCC_ShaderDebugInfoDXIL) {
      if (pDxil != nullptr) {
        FATAL_ERROR(FailedToLoadInput);
      }
      if (c->GetPartContent(i, &pDxil) != S_OK) {
        FATAL_ERROR(FailedToLoadInput);
      }
    }

#define DXIL_FOURCC(ch0, ch1, ch2, ch3) (                            \
  (uint32_t)(uint8_t)(ch0)        | (uint32_t)(uint8_t)(ch1) << 8  | \
  (uint32_t)(uint8_t)(ch2) << 16  | (uint32_t)(uint8_t)(ch3) << 24   \
  )
    static constexpr std::uint32_t DFCC_ShaderDebugInfoPDB = DXIL_FOURCC('S', 'P', 'D', 'B');
#undef DXIL_FOURCC

    if (DFCC == DFCC_ShaderDebugInfoPDB) {
      if (pPdb != nullptr) {
        FATAL_ERROR(FailedToLoadInput);
      }
      if (c->GetPartContent(i, &pPdb) != S_OK) {
        FATAL_ERROR(FailedToLoadInput);
      }
    }
  }

  *ppDxil = pDxil.Detach();
  *ppPdb = pPdb.Detach();
}

HRESULT CreateDxcDiaDataSource(_In_ REFIID riid, _Out_ LPVOID* ppv);

static HRESULT CreateDxcDiaDataSource(IDiaDataSource **DDS) {
  return CreateDxcDiaDataSource(__uuidof(IDiaDataSource), (void**)DDS);
}

static bool LooksLikeDxilProgramHeader(llvm::StringRef contents) {
  auto *dxil_program = (hlsl::DxilProgramHeader *)contents.data();
  return dxil_program->BitcodeHeader.DxilMagic == 0x4C495844;  // ASCI, 'DXIL'
}

static llvm::StringRef SkipDxilProgramHeader(llvm::StringRef program_header) {
  auto *dxil_program = (hlsl::DxilProgramHeader *)program_header.data();
  return llvm::StringRef(
      (const char*) (dxil_program + 1),
      dxil_program->SizeInUint32 * sizeof(std::uint32_t) - sizeof(*dxil_program));
}

void dxdia::LoadLLVMModule(
    llvm::StringRef Contents,
    llvm::StringRef BufId,
    llvm::LLVMContext *Ctx,
    llvm::SMDiagnostic *Err,
    IDiaDataSource **ppDataSource) {
  if (LooksLikeDxilProgramHeader(Contents)) {
    Contents = SkipDxilProgramHeader(Contents);
  }
  std::unique_ptr<llvm::Module> M = llvm::parseIR(
      {Contents, BufId},
      *Err, *Ctx);
  if (M == nullptr) {
    // Err.print(argv[0], llvm::errs());
    FATAL_ERROR(FailedToParseBC);
  }

  std::string serializedM;
  llvm::raw_string_ostream OS(serializedM);
  OS.SetUnbuffered();
  static constexpr bool ShouldPreserveUseListOrder = true;
  llvm::WriteBitcodeToFile(M.get(), OS, ShouldPreserveUseListOrder);

  CComPtr<DxDiaBuffer> DxilModule = DxDiaBuffer::Create(serializedM);

  CComPtr<IDiaDataSource> DS;
  if (CreateDxcDiaDataSource(&DS) != S_OK) {
    FATAL_ERROR(DataSourceCreationErr);
  }

  if (DS->loadDataFromIStream(DxilModule) != S_OK) {
    FATAL_ERROR(DataSourceLoadErr);
  }
  *ppDataSource = DS.Detach();
}

void dxdia::LoadPBD(
    llvm::StringRef input_filename,
    IDiaDataSource **ppDataSource) {
  *ppDataSource = nullptr;

  std::string tmp_pdb_filename;
  if (input_filename == "-") {
    // I could not for the life of me get IDiaDataSource::loadFromISession
    // to work with in-memory PDBs, but it is happy with the same PDBs when
    // they are loaded from disk, hence we load stdin to memory, and write it
    // to a temporary file.
    auto pdb_contents = llvm::MemoryBuffer::getSTDIN();
    if (!pdb_contents) {
      FATAL_ERROR(FailedToLoadInput);
    }
    CComPtr<DxDiaBuffer> pdb = DxDiaBuffer::Create(pdb_contents->get()->getBuffer());
    WritePDBToTmpFile(pdb, &tmp_pdb_filename);
    input_filename = tmp_pdb_filename;
  }

  CComPtr<IDiaDataSource> DDS;
  DDS.CoCreateInstance(__uuidof(DiaSource));
  if (!DDS) {
    FATAL_ERROR(DataSourceCreationErr);
  }

  if (DDS->loadDataFromPdb(_bstr_t(input_filename.data())) != S_OK) {
    FATAL_ERROR(DataSourceLoadErr);
  }
  *ppDataSource = DDS.Detach();
}
