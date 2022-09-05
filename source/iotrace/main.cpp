/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <memory>
#include <octf/cli/Executor.h>
#include <octf/interface/InterfaceConfigurationImpl.h>
#include <octf/interface/InterfaceTraceManagementImpl.h>
#include <octf/interface/InterfaceTraceParsingImpl.h>
#include <octf/utils/Exception.h>
#include "InterfaceKernelTraceCreatingImpl.h"

using namespace std;
using namespace octf;
using namespace octf::cli;

#define VALUE_TO_STRING(x) #x
#define TOSTR(x) VALUE_TO_STRING(x)

#define VALUE_TO_STRING(x) #x
#define TOSTR(x) VALUE_TO_STRING(x)

#ifdef IOTRACE_VERSION_LABEL
#define IOTRACE_VERSION_STRING \
    TOSTR(IOTRACE_VERSION) " (" TOSTR(IOTRACE_VERSION_LABEL) ")"
#else
#define IOTRACE_VERSION_STRING TOSTR(IOTRACE_VERSION)
#endif

int main(int argc, char *argv[]) {
    const string APP_NAME = "iotrace";
    try {
        // Create executor and local command set
        Executor ex;

        auto &properties = ex.getCliProperties();

        properties.setName(APP_NAME);
        properties.setVersion(IOTRACE_VERSION_STRING);

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

        // Configuration Interface for setting trace repository path
        InterfaceShRef iConfiguration =
                std::make_shared<InterfaceConfigurationImpl>();

        // Add interfaces to executor
        ex.addModules(iTraceManagement, iKernelTarcing, iTraceParsing,
                      iConfiguration);

        // Execute command
        return ex.execute(argc, argv);

    } catch (Exception &e) {
        log::cerr << e.what() << endl;
        return 1;
    } catch (std::exception &e) {
        log::critical << APP_NAME << " execution interrupted: " << e.what()
                      << endl;
        return 1;
    }
}
