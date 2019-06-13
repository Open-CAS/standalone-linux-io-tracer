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

static const char *get_version() {
    if (IOTRACE_VERSION_LABEL[0]) {
        return IOTRACE_VERSION "(" IOTRACE_VERSION_LABEL ")";
    } else {
        return IOTRACE_VERSION;
    }
}

int main(int argc, char *argv[]) {
    const string APP_NAME = "iotrace";
    CLIProperties::getCliProperties().setName(APP_NAME);
    CLIProperties::getCliProperties().setVersion(get_version());

    try {
        if (argc > 1) {
            // Create executor and local command set
            Executor ex;
            ex.addLocalCommand(make_shared<CmdVersion>());

            // Add interfaces as local commands
            // Trace Management Interface
            InterfaceShRef interfaceTraceManagement =
                    std::make_shared<InterfaceTraceManagementImpl>("");
            ex.addLocalModule(interfaceTraceManagement);

            // Kernel Trace Creating Interface
            std::vector<NodeId> nodePath{NodeId("kernel")};
            InterfaceShRef interfaceKernelTarcing =
                    std::make_shared<InterfaceKernelTraceCreatingImpl>(
                            nodePath);
            ex.addLocalModule(interfaceKernelTarcing);

            // Trace Parsing Interface
            InterfaceShRef interfaceTraceParsing =
                    std::make_shared<InterfaceTraceParsingImpl>();
            ex.addLocalModule(interfaceTraceParsing);

            // Parse application input
            vector<string> arguments(argv, argv + argc);
            CLIList cliList;
            cliList.create(arguments);

            // Execute command
            if (ex.execute(cliList) == false) {
                return 1;
            }

        } else {
            throw InvalidParameterException(
                    "Specify module or command first. Use '" +
                    CLIProperties::getCliProperties().getName() +
                    " -H' for help.");
        }

    } catch (Exception &e) {
        log::cerr << e.what() << endl;
        return 1;
    } catch (std::exception &e) {
        log::critical << APP_NAME << " execution interrupted: " << e.what()
                      << endl;
        return 1;
    }

    return 0;
}
