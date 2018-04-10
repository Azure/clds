# C Lockless Data Structures

`clds` is a general purpose C library implementing several lockless data structures.

Implemented lockless data structures:

- Singly linked list
- Hash table

*This library is work in progress*. The first release should happen once the first lockless structure is usable and working.

The goals are:

- Performant lockless data structures
- C11 compliant code.

## Dependencies

`clds` uses `azure-c-shared-utility`, which is a C library providing common functionality for basic tasks (string manipulation, list manipulation, logging, etc.).
`azure-c-shared-utility` is available here: https://github.com/Azure/azure-c-shared-utility and it is used as a submodule.

`clds` uses `ctest` as a test runner.
`clds` uses `umock-c` as mocking framework.

`clds` uses cmake for configuring build files.

## Setup

### Build

- Clone `clds` by:

```
git clone --recursive https://github.com/dcristoloveanu/clds.git
```

- Create a folder named `cmake` under `clds`

- Switch to the `cmake` folder and run

```
cmake ..
```

- Build

```
cmake --build .
```

### Building the tests

In order to build the unit tests use:

```
cmake .. -Drun_unittests:bool=ON
```

## Switching branches

After any switch of branches (git checkout for example), one should also update the submodule references by:

```
git submodule update --init --recursive
```
