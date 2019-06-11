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

    // Add interfaces as local commands
    // Trace Management Interface
    InterfaceShRef interfaceTraceManagement =
            std::make_shared<InterfaceTraceManagementImpl>("");
    ex.addLocalModule(interfaceTraceManagement);

    // Kernel Trace Creating Interface
    std::vector<NodeId> nodePath{NodeId("kernel")};
    InterfaceShRef interfaceKernelTarcing =
            std::make_shared<InterfaceKernelTraceCreatingImpl>(nodePath);
    ex.addLocalModule(interfaceKernelTarcing);

    // Trace Parsing Interface
    InterfaceShRef interfaceTraceParsing =
            std::make_shared<InterfaceTraceParsingImpl>();
    ex.addLocalModule(interfaceTraceParsing);

    // Execute command
    return ex.execute(argc, argv);
    ;
}
