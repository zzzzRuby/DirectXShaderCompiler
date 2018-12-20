# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
if(WIN32)

  find_package(DiaSDK REQUIRED) # Used for constants and declarations.

  set(LLVM_LINK_COMPONENTS
    BitWriter
	Core
    dxcsupport
    dxil
	IRReader
	DxilDia
	MSSupport
    Support
  )

  set(ARG_LINK_COMPONENTS
	${DIASDK_LIBRARIES}
  )

  add_llvm_tool(dxdia
    dxdia.cpp
	DxDiaInput.cpp
	DxDiaOutput.cpp
  )

  include_directories(AFTER ${LLVM_INCLUDE_DIR}/dxc/Tracing ${DIASDK_INCLUDE_DIRS})

endif(WIN32)