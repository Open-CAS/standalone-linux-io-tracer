/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <exception>
#include <memory>
#include <string>
#include <vector>
#include <octf/cli/CLIList.h>
#include <octf/cli/CLIProperties.h>
#include <octf/cli/CLIUtils.h>
#include <octf/cli/CommandSet.h>
#include <octf/cli/Executor.h>
#include <octf/cli/cmd/CmdHelp.h>
#include <octf/cli/cmd/CmdVersion.h>
#include <octf/cli/cmd/Command.h>
#include <octf/cli/cmd/CommandProtobuf.h>
#include <octf/cli/cmd/ICommand.h>
#include <octf/cli/param/ParamNumber.h>
#include <octf/interface/InterfaceTraceManagementImpl.h>
#include <octf/interface/InterfaceTraceParsingImpl.h>
#include <octf/node/INode.h>
#include <octf/utils/Exception.h>
#include <octf/utils/Log.h>
#include "InterfaceKernelTraceCreatingImpl.h"

using namespace std;
using namespace octf;

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

    // ex.addLocalModule(iTraceParsing);

    // Execute command
    return ex.execute(argc, argv);
}
