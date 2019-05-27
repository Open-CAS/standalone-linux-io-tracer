/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

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
#include <exception>
#include <memory>
#include <string>
#include <vector>
#include "InterfaceKernelTraceCreatingImpl.h"

using namespace std;
using namespace octf;

int main(int argc, char *argv[]) {
    const string APP_NAME = "iotrace";
    CLIProperties::getCliProperties().setName(APP_NAME);
    CLIProperties::getCliProperties().setVersion(IOTRACE_VERSION);

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
            ex.execute(cliList);

        } else {
            throw InvalidParameterException(
                    "Specify module or command first. Use '" +
                    CLIProperties::getCliProperties().getName() +
                    " -H' for help.");
        }

    } catch (Exception &e) {
        log::cerr << e.what() << endl;
        return -EINVAL;
    } catch (std::exception &e) {
        log::critical << APP_NAME << " execution interrupted: " << e.what()
                      << endl;
        return -EINVAL;
    }

    return 0;
}
