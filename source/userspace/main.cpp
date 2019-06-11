/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <memory>
#include <octf/cli/Executor.h>
#include <octf/interface/InterfaceTraceManagementImpl.h>
#include <octf/interface/InterfaceTraceParsingImpl.h>
#include "InterfaceKernelTraceCreatingImpl.h"

using namespace std;
using namespace octf;
using namespace octf::cli;

int main(int argc, char *argv[]) {
    // Create executor and local command set
    Executor ex;

    auto &properties = ex.getCliProperties();

    const string APP_NAME = "iotrace";
    properties.setName(APP_NAME);
    properties.setVersion(IOTRACE_VERSION);

    // Create interfaces

    // Trace Management Interface
    InterfaceShRef iTraceManagement =
            std::make_shared<InterfaceTraceManagementImpl>("");

    // Kernel Trace Creating Interface
    InterfaceShRef iKernelTarcing =
            std::make_shared<InterfaceKernelTraceCreatingImpl>();

    // Trace Parsing Interface
    InterfaceShRef iTraceParsing =
            std::make_shared<InterfaceTraceParsingImpl>();

    // Add interfaces to executor
    ex.addInterfaces({iTraceManagement, iKernelTarcing, iTraceParsing});

    // Execute command
    return ex.execute(argc, argv);
}
