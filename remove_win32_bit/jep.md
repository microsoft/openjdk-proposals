# JEP NNN: Removal of Windows 32-bit Support

_Author_	Bruno Borges & George Adams
_Owner_ Bruno Borges
_Type_	Feature
_Scope_	Implementation
_Status_	Draft
_Release_	23
_Component_	hotspot / other
_Discussion_	jdk dash dev at openjdk dot org
_Effort_	?
_Duration_	?
_Reviewed by_	Magnus Ihse Bursie
_Endorsed by_	Magnus Ihse Bursie
_Created_	<date>
_Updated_	<date>
_Issue_	<issue#>

Summary
-------

Following the deprecation approach in JEP 449, this proposal aims to take a further step by completely removing Windows 32-bit support from the OpenJDK. 
This decision acknowledges the evolving landscape of computing, where 64-bit systems have become predominant. 
The removal of 32-bit support will streamline development and focus resources on enhancing the performance and security of OpenJDK on modern architectures.

Goals
-----

- To cease all testing and development efforts targeting the Windows 32-bit platform within the OpenJDK project.
- To simplify OpenJDK's build and test infrastructure by focusing solely on Windows 64-bit support, aligning with current computing standards.

Non-Goals
---------

- This JEP does not aim to remove 32-bit support for platforms other than Windows.
- It does not intend to eliminate any 32-bit specific code paths that may exist that do not impact the Windows 32-bit platform.
- It does not intend to eliminate code on previous versions of OpenJDK.

Motivation
----------

Building on the rationale of JEP 449, which deprecated the Windows 32-bit installer, this proposal recognizes the continued decline in the use and necessity of 32-bit Java applications on Windows.
The computing industry has largely transitioned to 64-bit architectures, making the maintenance of a 32-bit JVM for Windows increasingly impractical and resource-intensive. 
By removing this support, OpenJDK can allocate more resources towards innovations and improvements that benefit the broader community.

Description
-----------

The execution of this proposal will involve:

1. **Build and Test Infrastructure**: Modifications to the OpenJDK build system to remove support for compiling on Windows 32-bit platforms, along with a halt in all testing activities for this architecture.
1. **Documentation and Communication**: Updates to OpenJDK documentation to reflect the removal of Windows 32-bit support and active communication within the community to ensure a smooth transition for users and developers.

Reference
---------

This proposal is inspired by and builds upon [JEP 449](https://openjdk.org/jeps/449), which deprecated the 32-bit Windows installer of OpenJDK.

Risks and Assumptions
---------------------

- **Risk**: There may be a subset of the user base that relies on 32-bit Java applications on Windows. Transition guidance and support by distributions and vendors of OpenJDK binaries will be critical.
- **Assumption**: Most current Java applications and users on Windows can migrate to 64-bit without significant obstacles, minimizing disruption.

Dependencies
------------

This JEP is independent of other proposals and is solely focused on the removal of 32-bit support for Windows within OpenJDK.

Impact
------

- **Compatibility**: This change necessitates that applications running on 32-bit Java on Windows migrate to 64-bit Java or remain on legacy versions of OpenJDK that still include 32-bit support.
- **Testing and Development**: Focusing on 64-bit architectures will allow for more dedicated testing and development efforts, potentially improving OpenJDK's performance and security on these platforms.
- **Documentation**: Documentation will be updated to clearly state the removal of support for Windows 32-bit environments.
