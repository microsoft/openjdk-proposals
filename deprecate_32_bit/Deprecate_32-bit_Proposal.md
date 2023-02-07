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
Dropping support for this port will enable contributors in the OpenJDK Community to accelerate the development of new features that will move the platform forward. Windows 10 (the last Windows operating system to support a 32-bit installation) will reach EOL on October 14, 2025, and I believe a major part of Java workloads on Windows 32-bit is running older versions of Java without any forecast for upgrading to modern, post OpenJDK 17, versions.
Another motivation factor is that there is currently no implementation of [JEP 436 (Virtual Threads)](https://openjdk.org/jeps/436) for 32-bit platforms and without a vendor stepping forward to implement this it's unlikelty that vendors will be able to continue supporting the platform.
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

Just as with [JEP 362 (Deprecate the Solaris and SPARC Ports)](https://openjdk.org/jeps/362), an alternative is for a set of credible developers to express a clear desire to maintain this port going forward including but not limited to creating an implementation of Virtual Threads for x86-32. If that happens before this JEP is integrated then this JEP can be withdrawn. If that happens after this JEP is integrated, but before the ports are removed, then a follow-on JEP can revert the deprecation.
