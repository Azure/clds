// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#define logger_init mock_logger_init
#define logger_deinit mock_logger_deinit

// Since we mightbe running in an environment where a DllMain exists (like running under VS CppUnittest)
// we want to rename it to something else
#define DllMain DllMain_under_test

#include "../../src/thread_notifications_lackey_dll.c"

