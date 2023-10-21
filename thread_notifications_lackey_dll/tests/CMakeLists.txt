#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#unittests
if(${run_unittests})
    build_test_folder(thread_notifications_lackey_dll_ut)
    build_test_folder(thread_notif_lackey_dll_wo_init_ut)
endif()

#inttests
if(${run_int_tests})
    build_test_folder(thread_notifications_lackey_dll_int)
endif()
