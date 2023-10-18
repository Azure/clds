# `thread_notifications_lackey_dll` requirements

## Overview

`thread_notifications_lackey_dll` is module that implements `DllMain` in order to hook thread attach/detach notifications.

It is named "lackey" because that is what our friend Raymond called it :-).

Its goal is to abstract the implementation of a Dll in order to be able to hook DLL thread attach/detach notifications (`DllMain` is not accesible in executables).

Only one callback with no context is supported. If multiple consumers of callbacks are needed, that functionality would be the responsibility of a separate unit that could demux the callbacks and have contexts stored.

## References

[https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain)

[Fibers aren’t useful for much any more; there’s just one corner of it that remains useful for a reason unrelated to](https://devblogs.microsoft.com/oldnewthing/20191011-00/?p=102989)

## Exposed API

```c
#define THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES \
    THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_ATTACH, \
    THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_DETACH

MU_DEFINE_ENUM(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES)

typedef void (*THREAD_NOTIFICATION_LACKEY_DLL_CALLBACK_FUNC)(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason);

MOCKABLE_FUNCTION(, int, thread_notifications_lackey_dll_init_callback, THREAD_NOTIFICATION_LACKEY_DLL_CALLBACK_FUNC, thread_notification_cb);
MOCKABLE_FUNCTION(, int, thread_notifications_lackey_dll_deinit_callback);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
```

### thread_notifications_lackey_dll_set_callback

```c
MOCKABLE_FUNCTION(, int, thread_notifications_lackey_dll_set_callback, THREAD_NOTIFICATION_LACKEY_DLL_CALLBACK_FUNC, thread_notification_cb, void*, context);
```

`thread_notifications_lackey_set_callback` sets the callback/context that is to be called whenever a thread attach/detach event happens.

If `thread_notification_cb` is `NULL`, `thread_notifications_lackey_set_callback` shall fail and return a non-zero value.

Otherwise, `THREAD_NOTIFICATIONS_LACKEY_dll_init` shall replace the previously used callback information maintained by the module with `thread_notification_cb`, succeed and return 0.

### thread_notifications_lackey_dll_clear_callback

```c
MOCKABLE_FUNCTION(, int, THREAD_NOTIFICATIONS_LACKEY_dll_clear_callback);
```

`THREAD_NOTIFICATIONS_LACKEY_dll_deinit` clears the thread notifications callback.

`THREAD_NOTIFICATIONS_LACKEY_dll_deinit` shall set the callback maintained by the module to `NULL`.

### DllMain

```c
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
```

`DllMain` implements the DLL main entry point.

If `fdwReason` is `DLL_PROCESS_ATTACH`:

- `DllMain` shall call `logger_init` to initialize logging for the module.

- If `logger_init` fails, `DllMain` shall return FALSE.

- Otherwise, `DllMain` shall initialize the callback maintained by the module wth `NULL` and return `TRUE`.

If `fdwReason` is `DLL_PROCESS_DETACH`:

- `DllMain` shall call `logger_deinit` and return `TRUE`.

If `fdwReason` is `DLL_THREAD_ATTACH`:

- If a callback was passed by the user through a `thread_notifications_lackey_dll_init_callback` call, the callback shall be called with `THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_ATTACH`.

- `DllMain` shall return `TRUE`.

If `fdwReason` is `DLL_THREAD_DETACH`:

- If a callback was passed by the user through a `thread_notifications_lackey_dll_init_callback` call, the callback shall be called with `THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_DETACH`.

- `DllMain` shall return `TRUE`.

If `fdwReason` is any other value, `DllMain` shall terminate the process.
