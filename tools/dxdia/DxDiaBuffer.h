#pragma once

#include "dxc/Support/WinIncludes.h"

#include <string>

#include <d3dcompiler.h>

#include "dxc/dxcapi.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/microcom.h"
#include "llvm/ADT/StringRef.h"

#include "DxDiaResult.h"

namespace dxdia {
struct DxDiaBuffer : public IDxcBlob, public IStream {
  DxDiaBuffer(const DxDiaBuffer &) = delete;
  DxDiaBuffer &operator =(const DxDiaBuffer &) = delete;
  DXC_MICROCOM_TM_REF_FIELDS()
  CComPtr<ID3DBlob> D3DBlob;
  CComPtr<IDxcBlob> DxcBlob;
  std::string Contents;
  llvm::StringRef Value;

  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxDiaBuffer)

  void assign(CComPtr<ID3DBlob> B) {
    D3DBlob = B;
    DxcBlob.Release();
    Contents.clear();
    Value = llvm::StringRef(reinterpret_cast<const char *>(B->GetBufferPointer()),
                                                           B->GetBufferSize());
  }

  void assign(CComPtr<IDxcBlob> B) {
    D3DBlob.Release();
    DxcBlob = B;
    Contents.clear();
    Value = llvm::StringRef(reinterpret_cast<const char *>(B->GetBufferPointer()),
                            B->GetBufferSize());
  }

  void assign(llvm::StringRef str) {
    D3DBlob.Release();
    DxcBlob.Release();
    Contents = str;
    Value = Contents;
  }

  template <typename VT>
  static CComPtr<DxDiaBuffer> CreateWithMalloc(IMalloc *Malloc, VT value) {
    CComPtr<DxDiaBuffer> ppv = CreateOnMalloc<DxDiaBuffer>(Malloc);
    if (ppv == nullptr) {
      FATAL_ERROR(OOM);
    }
    ppv->assign(value);
    return ppv;
  }

  template <typename VT>
  static CComPtr<DxDiaBuffer> Create(VT value) {
    return CreateWithMalloc(DxcGetThreadMallocNoRef(), value);
  }

  STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcBlob, IStream>(this, iid, ppvObject);
  }

#pragma region IDxcBlob implementation
  LPVOID STDMETHODCALLTYPE GetBufferPointer(void) override {
    return (LPVOID) Value.data();
  }

  SIZE_T STDMETHODCALLTYPE GetBufferSize(void) override {
    return Value.size();
  }
#pragma endregion

  static HRESULT ENotImpl() { return E_NOTIMPL; }

#pragma region IStream implementation.
  STDMETHODIMP Read(
    /* [annotation] */
    _Out_writes_bytes_to_(cb, *pcbRead)  void *pv,
    /* [annotation][in] */
    _In_  ULONG cb,
    /* [annotation] */
    _Out_opt_  ULONG *pcbRead) override { return ENotImpl(); }

  STDMETHODIMP Write(
    /* [annotation] */
    _In_reads_bytes_(cb)  const void *pv,
    /* [annotation][in] */
    _In_  ULONG cb,
    /* [annotation] */
    _Out_opt_  ULONG *pcbWritten) override { return ENotImpl(); }

  STDMETHODIMP Seek(
    /* [in] */ LARGE_INTEGER dlibMove,
    /* [in] */ DWORD dwOrigin,
    /* [annotation] */
    _Out_opt_  ULARGE_INTEGER *plibNewPosition) override { return ENotImpl(); }

  STDMETHODIMP SetSize(
    /* [in] */ ULARGE_INTEGER libNewSize) override { return ENotImpl(); }

  STDMETHODIMP CopyTo(
    /* [annotation][unique][in] */
    _In_  IStream *pstm,
    /* [in] */ ULARGE_INTEGER cb,
    /* [annotation] */
    _Out_opt_  ULARGE_INTEGER *pcbRead,
    /* [annotation] */
    _Out_opt_  ULARGE_INTEGER *pcbWritten) override { return ENotImpl(); }

  STDMETHODIMP Commit(
    /* [in] */ DWORD grfCommitFlags) override { return ENotImpl(); }

  STDMETHODIMP Revert(void) override { return ENotImpl(); }

  STDMETHODIMP LockRegion(
    /* [in] */ ULARGE_INTEGER libOffset,
    /* [in] */ ULARGE_INTEGER cb,
    /* [in] */ DWORD dwLockType) override { return ENotImpl(); }

  STDMETHODIMP UnlockRegion(
    /* [in] */ ULARGE_INTEGER libOffset,
    /* [in] */ ULARGE_INTEGER cb,
    /* [in] */ DWORD dwLockType)  override { return ENotImpl(); }

  STDMETHODIMP Stat(
    /* [out] */ __RPC__out STATSTG *pstatstg,
    /* [in] */ DWORD grfStatFlag) override { return ENotImpl(); }

  STDMETHODIMP Clone(
    /* [out] */ __RPC__deref_out_opt IStream **ppstm) override { return ENotImpl(); }
#pragma endregion
};
}  // namespace dxdia
