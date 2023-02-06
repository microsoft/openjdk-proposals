Summary
-------
 
Deprecate the Windows x86-32 port, with the intent to remove them in a future release.

Goals
-----
The proposal has the following goals: 
1. Enhance the build system to issue an error message when an attempt is made to configure a build for one of the deprecated port. The error message will be suppressible via a new configuration option.
1. Mark the port, and related port-specific features, as deprecated for removal in the relevant JDK documentation.

Non-Goals
---------
1. It is not a goal of this JEP to change the status of the affected port in any prior release. The earliest release to which this JEP could be targeted is JDK 21.

 
Motivation
----------
Dropping support for this port will enable contributors in the OpenJDK Community to accelerate the development of new features that will move the platform forward. Windows 10 (the last Windows operating system to support a 32-bit installation) will reach EOL on October 14, 2025. With Java 21 being potentially supported up to September 2031, it makes sense to deprecate the 32-bit port from the codebase.
Another motivation factor is that there is currently no implementation of [JEP 436: Virtual Threads](https://openjdk.org/jeps/436) for 32-bit platforms and without a vendor stepping forward to implement this it's unlikelty that vendors will be able to continue supporting the platform.
The main focus of this JEP is to deprecate the Windows x86 port but this JEP could also extend deprecate Linux at the same time.

Description
-----------
Build-configuration changes
An attempt to configure a Windows x86-32 build will produce the following output:

```bash
$ bash ./configure
...
checking compilation type... native
configure: error: The Windows x86-32 port is deprecated and may be removed in a future release. \
Use --enable-deprecated-ports=yes to suppress this error.
configure exiting with result code 1
$
```

The new build-configuration option --enable-deprecated-ports=yes will suppress the error and continue:

```bash
$ bash ./configure --enable-deprecated-ports=yes
...
checking compilation type... native
configure: WARNING: The Windows x86-32 port is deprecated and may be removed in a future release.
...
Build performance summary:
* Cores to use:   32
* Memory limit:   96601 MB

The following warnings were produced. Repeated here for convenience:
WARNING: The Windows x86-32 port is deprecated and may be removed in a future release.
$
```

The error/warning will be issued when configuring a build for Windows x86.

Alternatives
-----------

TBD
