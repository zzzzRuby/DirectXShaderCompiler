#pragma once

#include <string>

#include "dia2.h"

namespace dxdia {

#define ST_ENUM(X) \
  X(Null) \
  X(Exe) \
  X(Compiland) \
  X(CompilandDetails) \
  X(CompilandEnv) \
  X(Function) \
  X(Block) \
  X(Data) \
  X(Annotation) \
  X(Label) \
  X(PublicSymbol) \
  X(UDT) \
  X(Enum) \
  X(FunctionType) \
  X(PointerType) \
  X(ArrayType) \
  X(BaseType) \
  X(Typedef) \
  X(BaseClass) \
  X(Friend) \
  X(FunctionArgType) \
  X(FuncDebugStart) \
  X(FuncDebugEnd) \
  X(UsingNamespace) \
  X(VTableShape) \
  X(VTable) \
  X(Custom) \
  X(Thunk) \
  X(CustomType) \
  X(ManagedType) \
  X(Dimension) \
  X(CallSite) \
  X(InlineSite) \
  X(BaseInterface) \
  X(VectorType) \
  X(MatrixType) \
  X(HLSLType) \
  X(Caller) \
  X(Callee) \
  X(Export) \
  X(HeapAllocationSite) \
  X(CoffGroup) \
  X(Inlinee) \
  X(Max)

static std::wstring SymTagToStr(decltype(SymTagNull) ST) {
  switch (ST)
  {
  default:
    return L"<unknown " + std::to_wstring(ST) + L">";
#define CASE(EnumName) case SymTag ## EnumName: return L#EnumName;
    ST_ENUM(CASE)
#undef CASE
    break;
  }
}

#undef ST_ENUM
}  // namespace dxdia
