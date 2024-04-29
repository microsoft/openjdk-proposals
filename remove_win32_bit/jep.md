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

Remove the source code and build support for the Windows 32-bit x86 port. This port was deprecated for removal in JDK 21 with the express intent to remove them in a future release.

Goals
-----

- To remove all code paths in the code base that apply only to Windows 32-bit x86.
- To cease all testing and development efforts targeting the Windows 32-bit x86 platform.
- To simplify JDK's build and test infrastructure, aligning with current computing standards.

Non-Goals
---------

- This JEP does not aim to remove or change 32-bit support for any platforms other than Windows.
- This JEP does not aim to eliminate code or support for Windows 32-bit on previous versions of the JDK.

Motivation
----------

- Allow contributors in the OpenJDK community to accelerate the development of new features and enhancements that will move the platform forward.

- The implementation of [JEP 436 (Virtual Threads)](https://openjdk.org/jeps/436) for Windows x86-32 falls back to the use of kernel threads and therefore does not bring the expected benefits of Project Loom.

- Windows 10, the last Windows operating system to support 32-bit operation, will reach End of Life in [October 2025](https://learn.microsoft.com/lifecycle/products/windows-10-home-and-pro).

Description
-----------

The execution of this proposal will involve:

1. **Code removal**: Find and remove all code paths in the code base that apply only to Windows 32-bit.
1. **Build and Test Infrastructure**: Modifications to the JDK build system to remove support for compiling on Windows 32-bit platforms, along with a halt in all testing activities for this architecture.
1. **Documentation and Communication**: Updates to JDK documentation to reflect the removal of Windows 32-bit support and active communication within the community to ensure a smooth transition for users and developers.

Risks and Assumptions
---------------------

- **Risk**: There may be a subset of the user base that relies on 32-bit Java applications on Windows. This change necessitates that applications running on 32-bit Java on Windows migrate to a 64-bit Java and Windows environment, or remain on legacy versions of the JDK - any version prior to JDK 23 - that still include 32-bit support. Transition guidance and support by distributions and vendors of JDK binaries will be critical.
- **Assumption**: Most current Java applications and users on Windows can migrate to 64-bit without significant obstacles, minimizing disruption.
