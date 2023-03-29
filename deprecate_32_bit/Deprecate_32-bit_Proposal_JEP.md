Summary
-------

Deprecate the Windows x86-32 port, with the intent to remove it in a future release.

Goals
-----

The proposal has the following goals:

1. Update the build system to issue an error message when an attempt is made to configure a build for the deprecated port. The error message will be suppressible via a new configuration option.
1. Mark the port, and related port-specific features, as deprecated for removal in the relevant JDK documentation.

Non-Goals
---------

1. It is not a goal of this JEP to change the status of the affected port in any prior release. The earliest release to which this JEP could be targeted is JDK 21.
1. It is not a goal of this JEP to deprecate any other 32-bit port.

Motivation
----------

Dropping support for this port will allow contributors in the OpenJDK community to accelerate the development of new features and enhancements that will move the platform forward. Currently, the implementation of [JEP 436 (Virtual Threads)][j436] for Windows 32-bit falls back to the use of kernel threads and therefore does not bring the expected benefits proposed by Project Loom.

Another motivation is that Windows 10 (the last Windows operating system to support a 32-bit installation) will reach End of Life (EOL) on October 14, 2025 [1][1].

Description
-----------

### Build-configuration changes

An attempt to configure a Windows x86-32 build will produce the following output:

```
$ bash ./configure
...
checking compilation type... native
configure: error: The Windows x86-32 port is deprecated and may be removed in a future release. \
Use --enable-deprecated-ports=yes to suppress this error.
configure exiting with result code 1
$
```

The new build-configuration option `--enable-deprecated-ports=yes` will suppress the error and continue:

```
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

The error/warning will be issued when configuring a build for Windows x86-32.

A proposed [webrev][webrev] is available showing the 3 changes (total 22 lines) to the following files:

- `doc/building.html`
- `doc/building.md`
- `make/autoconf/platform.m4`

Alternatives
------------

An alternative for this JEP is to give continuity of OpenJDK supportability for Windows 32-bit. This requires a better implementation of JEP 436 Virtual Threads, and future JEPs, to ensure that OpenJDK on Windows 32-bit remains meeting the expectations of Java developers, for as long as there is interest in the Java ecosystem for running newer JVMs on Windows 32-bit environments.

Risks and Assumptions
---------------------

It is known that 32-bit JVMs on Windows are still used due to integration with 32-bit DLLs. These users cannot migrate directly to 64-bit JVMs, because a 64-bit process on Windows cannot load a 32-bit DLL. While Windows 64-bit is capable of running 32-bit applications by emulating a 32-bit environment through WOW64 [2][2], applications will suffer from dramatic performance degradation [3][3] despite the assumed memory footprint benefits.

This JEP assumes that:

1. Users can continue to run existing builds of OpenJDK 32-bit JVM to integrate with native 32-bit libraries, and expose their functionality over remote API to be be consumed by applications running on a 64-bit JVM within the same environment
1. Legacy systems are unlikely to migrate to versions of OpenJDK following the release of Java 21.

[1]: https://learn.microsoft.com/lifecycle/products/windows-10-home-and-pro
[2]: https://learn.microsoft.com/en-us/windows/win32/winprog64/running-32-bit-applications
[3]: https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/compatibility-limitations-32-bit-programs-64-bit-system#program-performance-considerations
[j362]: https://openjdk.org/jeps/362
[j436]: https://openjdk.org/jeps/436
[webrev]: https://microsoft.github.io/openjdk-proposals/deprecate_32_bit/webrev/
