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

Dropping support for this port will allow contributors in the OpenJDK community to accelerate the development of new features and enhancements that will move the platform forward. Currently the implementation of [JEP 436 (Virtual Threads)](https://openjdk.org/jeps/436) for Windows 32-bit falls back to the use of kernel threads and therefore does not bring the expected benefits proposed by Project Loom.

Another motivation is that Windows 10 (the last Windows operating system to support a 32-bit installation) will reach End of Life (EOL) on October 14, 2025 [1][1].

[1]: https://learn.microsoft.com/lifecycle/products/windows-10-home-and-pro

The main focus of this JEP is to deprecate the Windows x86 port.

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

Just as with [JEP 362 (Deprecate the Solaris and SPARC Ports)](https://openjdk.org/jeps/362), an alternative is for a set of credible developers to express a clear desire to commit to and maintain this port going forward including but not limited to creating a proper implementation of Virtual Threads for x86-32. If this happens after this JEP is integrated, but before the ports are removed, then a follow-on JEP can revert the deprecation. If this happens before this JEP is integrated then this JEP can be withdrawn.

It is also known that 32-bit JVMs on Windows are still present in the wild due to integration with 32-bit DLLs. On Windows, 64-bit processes cannot load 32-bit DLLs. Alternatively, users can run an older version of a 32-bit JVM to integrate with these native libraries, and expose their functionality over some form of remote API that can be consumed by a newer 64-bit JVM, within the same environment.

Risks and Assumptions
---------------------

Windows 64-bit is capable of running 32-bit applications by emulating a 32-bit environment. This requires that 32-bit processes become isolated away from 64-bit processes. It is known that users can run a 32-bit JVM on a Windows 64-bit environment thanks to WOW64 [2][2], but at a dramatic performance cost [3][3] despite the assumed memory footprint benefits.

This JEP assumes that these legacy systems are unlikely to need the features present in newer versions of OpenJDK following the release of Java 21.

[2]: https://learn.microsoft.com/en-us/windows/win32/winprog64/running-32-bit-applications
[3]: https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/compatibility-limitations-32-bit-programs-64-bit-system#program-performance-considerations
