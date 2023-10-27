// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#define clds_sorted_list_create real_clds_sorted_list_create
#define clds_sorted_list_destroy real_clds_sorted_list_destroy
#define clds_sorted_list_insert real_clds_sorted_list_insert
#define clds_sorted_list_delete_item real_clds_sorted_list_delete_item
#define clds_sorted_list_delete_key real_clds_sorted_list_delete_key
#define clds_sorted_list_remove_key real_clds_sorted_list_remove_key
#define clds_sorted_list_find_key real_clds_sorted_list_find_key
#define clds_sorted_list_set_value real_clds_sorted_list_set_value
#define clds_sorted_list_lock_writes real_clds_sorted_list_lock_writes
#define clds_sorted_list_unlock_writes real_clds_sorted_list_unlock_writes
#define clds_sorted_list_get_count real_clds_sorted_list_get_count
#define clds_sorted_list_get_all real_clds_sorted_list_get_all

#define clds_sorted_list_node_create real_clds_sorted_list_node_create
#define clds_sorted_list_node_inc_ref real_clds_sorted_list_node_inc_ref
#define clds_sorted_list_node_release real_clds_sorted_list_node_release
