/**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 */
/*
 * ps_command.c
 *
 *  Created on: Aug 13, 2010
 *      Author: alexanderb
 */
#include <stdlib.h>

#include "command_private.h"
#include "array_list.h"
#include "bundle_context.h"
#include "bundle_archive.h"
#include "module.h"
#include "bundle.h"

char * psCommand_stateString(BUNDLE_STATE state);
void psCommand_execute(COMMAND command, char * line, void (*out)(char *), void (*err)(char *));

COMMAND psCommand_create(BUNDLE_CONTEXT context) {
	COMMAND command = (COMMAND) malloc(sizeof(*command));
	command->bundleContext = context;
	command->name = "ps";
	command->shortDescription = "list installed bundles.";
	command->usage = "ps [-l | -s | -u]";
	command->executeCommand = psCommand_execute;
	return command;
}

void psCommand_destroy(COMMAND command) {
	free(command);
}

void psCommand_execute(COMMAND command, char * commandline, void (*out)(char *), void (*err)(char *)) {
	ARRAY_LIST bundles = NULL;
	celix_status_t status = bundleContext_getBundles(command->bundleContext, &bundles);

	if (status == CELIX_SUCCESS) {
		bool showLocation = false;
		bool showSymbolicName = false;
		bool showUpdateLocation = false;
		char * msg = "Name";

		char delims[] = " ";
		char * sub = NULL;
		sub = strtok(commandline, delims);
		sub = strtok(NULL, delims);
		while (sub != NULL) {
			if (strcmp(sub, "-l") == 0) {
				showLocation = true;
				msg = "Location";
			} else if (strcmp(sub, "-s") == 0) {
				showSymbolicName = true;
				msg = "Symbolic name";
			} else if (strcmp(sub, "-u") == 0) {
				showUpdateLocation = true;
				msg = "Update location";
			}
			sub = strtok(NULL, delims);
		}

		char line[256];
		sprintf(line, "  %-5s %-12s %s\n", "ID", "State", msg);
		int i;
		out(line);
		for (i = 0; i < arrayList_size(bundles); i++) {
			BUNDLE bundle = arrayList_get(bundles, i);
			BUNDLE_ARCHIVE archive = NULL;
			long id;

			bundle_getArchive(bundle, &archive);
			bundleArchive_getId(archive, &id);
			BUNDLE_STATE state;
			bundle_getState(bundle, &state);
			char * stateString = psCommand_stateString(state);
			MODULE module = NULL;
			char * name = NULL;
			bundle_getCurrentModule(bundle, &module);
			module_getSymbolicName(module, &name);
			if (showLocation) {
				bundleArchive_getLocation(archive, &name);
			} else if (showSymbolicName) {
				// do nothing
			} else if (showUpdateLocation) {
				bundleArchive_getLocation(archive, &name);
			}

			sprintf(line, "  %-5ld %-12s %s\n", id, stateString, name);
			out(line);
		}
		arrayList_destroy(bundles);
	}
}

char * psCommand_stateString(BUNDLE_STATE state) {
	switch (state) {
		case BUNDLE_ACTIVE:
			return "Active      ";
		case BUNDLE_INSTALLED:
			return "Installed   ";
		case BUNDLE_RESOLVED:
			return "Resolved    ";
		case BUNDLE_STARTING:
			return "Starting    ";
		case BUNDLE_STOPPING:
			return "Stopping    ";
		default:
			return "Unknown     ";
	}
}
