# JEP NNN: Remove the Windows 32-bit x86 Port

_Author_ Bruno Borges & George Adams

_Owner_ Bruno Borges

_Type_ Feature

_Scope_ Implementation

_Status_ Draft

_Release_ 23

_Component_ hotspot / other

_Discussion_ jdk dash dev at openjdk dot org

_Effort_ ?

_Duration_ ?

_Reviewed by_ Magnus Ihse Bursie

_Endorsed by_ Magnus Ihse Bursie

_Created_ <date>

_Updated_ <date>

_Issue_ <issue#>

Summary
-------

Remove the source code and build support for the Windows 32-bit x86 port. This port was deprecated for removal in JDK 21 with the express intent to remove it in a future release.

Goals
-----

- Remove all code paths that apply only to Windows 32-bit x86.
- Cease all testing and development efforts targeting the Windows 32-bit x86 platform.
- Simplify the JDK's build and test infrastructure.

Non-Goals
---------

- It is not a goal to remove or change 32-bit support for any platforms other than Windows.
- It is not a goal to remove either code or support for Windows 32-bit in previous releases.

Motivation
----------

- Allow contributors in the OpenJDK Community to accelerate the development of new features and enhancements that will move the platform forward.
- The implementation of [JEP 436 (Virtual Threads)](https://openjdk.org/jeps/436) for Windows x86-32 falls back to the use of kernel threads and therefore does not bring the expected benefits of Project Loom.
- Windows 10, the last Windows operating system to support 32-bit operation, will reach End of Life in [October 2025](https://learn.microsoft.com/lifecycle/products/windows-10-home-and-pro).

Description
-----------

- Find and remove all code paths in the code base that apply only to Windows 32-bit.
- Modify the JDK build system to remove support for compiling on Windows 32-bit platforms, and halt testing activities for this architecture.
- Update the JDK documentation to reflect the removal of Windows 32-bit support, and publicize this change so as to ensure a smooth transition for users and developers.

Risks and Assumptions
---------------------

Some users may still rely on 32-bit Java applications on Windows. This change requires Java applications running on 32-bit Windows to migrate to a 64-bit JDK and Windows environment, or else remain on legacy versions of the JDK, prior to JDK 23, which still include 32-bit support. Transition guidance and support by distributions and vendors of JDK binaries will be critical.
