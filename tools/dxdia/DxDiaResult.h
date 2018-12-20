#pragma once

#include <cstdint>

namespace dxdia {

#define DXDIA_ERROR_LIST(X)         \
    X(FailedToLoadInput)            \
    X(D3DGetDebugInfoFailed)        \
    X(HLSLCompilationFailure)       \
    X(FailedToParseBC)              \
    X(OOM)                          \
    X(ThreadFSCreationErr)          \
    X(DataSourceCreationErr)        \
    X(DataSourceLoadErr)            \
    X(SessionCreationErr)           \
    X(QueryErr)                     \
    X(ChildrenEnumErr)              \
    X(D3DCompilerLoadErr)           \
    X(CompilerLoadErr)              \
    X(ENotImpl)                     \
    X(TempFileCreationErr)          \
    X(OutputFileCreationErr)        \
    X(UnknownErr)

struct FatalError {
public:
  enum class Kind {
#define DECLARE_ENUM(name) name,
    DXDIA_ERROR_LIST(DECLARE_ENUM)
#undef DECLARE_ENUM
  };

  FatalError(Kind k, const char *f, std::uint32_t l)
      : kind(k), filename(f), lineno(l) {}

  const Kind kind;
  const char *filename;
  const std::uint32_t lineno;

  const char *kind_str() const {
    static constexpr char *value[] = {
#define DECLARE_ENUM_NAMESTR(name) #name,
    DXDIA_ERROR_LIST(DECLARE_ENUM_NAMESTR)
#undef DECLARE_ENUM_NAMESTR
    };
    return value[static_cast<std::uint32_t>(kind)];
  }
};

#define FATAL_ERROR(name) throw dxdia::FatalError(dxdia::FatalError::Kind::name, __FILE__, __LINE__)
}  // namespace dxdia
