Summary
-------

Introduce a new JVM flag to set a limit on the amount of native memory the JVM shall use, and therefore size its heap automatically, compared to the amount of available memory in a system dedicated to the JVM.

Goals
-----

Allow the JVM to calculate its heap size as a subtraction of a user-defined native memory limit from the available memory in the environment.

Non-Goals
---------

The JVM shall not calculate what a reserved native memory amount may look like. This is user input.

Success Metrics
---------------

The heap size of the JVM, plus the user-input reserved native memory, equals to nearly 100% of the amount of available memory in the dedicated system for the JVM process. The safe margin may vary between 5% and up to 100MB on the gap between total memory expected by the JVM, and the remaining of the memory in the system.

Motivation
----------

JVM ergonomics have been historically conservative with the assumption that systems have multiple processes, and therefore the JVM shall be fair with the resources available by not consuming them in their entirety. In more recent times though, there are systems with dedicated resources for a particular process, such as dedicated VMs and Containers. These do not follow the same assumption. Operations teams often do resource planning (memory and CPU) to one primary, or solely, process.

Setting heap size using percentages to achieve dynamic scalling is somewhat wasteful, given that native memory consumption will, for most cases, eventually hit a peak that won't require more memory no matter how large the heap may be. As an example, if a JVM is configured with a 75% `MaxRAMPercentage`, the remainder 25% will grow as well if the total available memory in system also grows, but the native memory consumption may be stalled at e.g. 256m.

This JEP allows the user to reverse the heap size calculation by letting it expand to whatever is left after the subtraction of native memory from the total available memory.

Description
-----------

// REQUIRED -- Describe the enhancement in detail: Both what it is and,
// to the extent understood, how you intend to implement it.  Summarize,
// at a high level, all of the interfaces you expect to modify or extend,
// including Java APIs, command-line switches, library/JVM interfaces,
// and file formats.  Explain how failures in applications using this
// enhancement will be diagnosed, both during development and in
// production.  Describe any open design issues.
//
// This section will evolve over time as the work progresses, ultimately
// becoming the authoritative high-level description of the end result.
// Include hyperlinks to additional documents as required.

This JEP introduces two new flags:

  -XX:MaxNativeMemory=<input>
  -XX:NativeMemoryLimitPolicy=<none|enforced>

Whereas `<input>` for memory amount follows the same formatting as other memory-related parameters, such as `-Xmx`. Examples: `10m`, `50m`, `1g`. This parameter, if not present, defaults to zero, and the JVM ergonomics shall estimate heap size as per existing implementation-specific heuristics.

The second flag allows the user to control what to do if the JVM does consume more native memory than its limit. If there is a limit set, the default value is `enforced`. If not, then `none`.

The limit, if set, shall be readable through the `java.lang.management.MemoryMXBean` object:

    memoryMXBean.getNonHeapMemoryUsage().getMax()

Combined with `-XX:NativeMemoryTracking` at development time -- or more specifically integration testing phase, or a QA/pre-prod environment -- where developers may identify a reasonable value as a starting point, it shall be more straightforward to do resource planning and memory allocation to JVM processes in production upon first-time deployments.

Alternatives
------------

Users may still set the maximum heap size manually or relative to the amount of available memory, although this still holds the scalability of memory allocation not ideal, and wasteful. Alternatively, users may use shell scripts to perform such inverted calculation to identify available memory, subtract expected native memory consumption, and then set the maximum heap size. The use of shell scripts for this purpose has been present for many years, but it is often error prone.

Testing
-------

Besides the usual unit tests, the native memory limit tests must also be done inside VMs and containers, and both for cgroups v1 and v2 environments.

Risks and Assumptions
---------------------

In environments with little memory available, the JVM may observe more OutOfMemoryErrors that will lead to increased frustrations. This JEP assumes that developers that have indeed set a native memory limit, will be conscious of their choice and adjust accordingly to the needs of the application as they test their environment for meeting the ideal resource allocation.

Dependencies
------------

No dependencies identified.
