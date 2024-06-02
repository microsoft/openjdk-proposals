Summary
-------

Introduce a new JVM flag to set a limit on the amount of native memory the JVM shall use.

Goals
-----

Allow the JVM to calculate its heap size as a subtraction of a user-defined native memory limit from the available memory in the environment.

Non-Goals
---------

The JVM will not guess what a native memory limit may look like. The value is either `-1` (default), or `user input`.

Success Metrics
---------------

The heap size of the JVM, plus the user-input reserved native memory, equals to nearly 100% of the amount of available memory in the dedicated system for the JVM process. The safe margin may vary between 5% and up to 100MB on the gap between total memory expected by the JVM, and the remaining of the memory in the system.

Motivation
----------

JVM ergonomics have been historically balanced with the assumption that systems have multiple processes, and therefore the JVM shall be fair with the resources available by not consuming them in their entirety. In more recent times though, there are environments with dedicated resources for a particular process, such as dedicated VMs and Containers. These do not follow the same past assumptions. Operations teams often do resource planning (memory and CPU) to one primary, or solely, process.

Setting heap size using percentages to achieve dynamic scalling is somewhat wasteful, given that native memory consumption will, for most cases, eventually hit a peak that won't require significantly more memory no matter how large the heap may be. As an example, if a JVM is configured with a 75% `MaxRAMPercentage` in a container with 4GB of RAM, and the container scales up to 6GB of RAM, the remainder 25% will grow as well but the native memory consumption may be stalled at e.g. 400m.

This JEP allows the user to reverse the heap size calculation by letting it expand to whatever is left after the subtraction of native memory from the total available memory.

Description
-----------

This JEP introduces the following new flags:

    -XX:MaxNativeMemory=<input>
    -XX:NativeMemoryLimitPolicy=<none|warn|enforced>
    -XX:+UseAutoHeapSizing
    -XX:NativeMemorySafeMargin=<input>

**_Set a native memory limit_**

The `<input>` for the amount follows the same formatting as other memory-related parameters, such as `-Xmx`. Examples: `10m`, `50m`, `1g`. This parameter, if not present, defaults to `-1`, and the JVM ergonomics shall estimate heap size as per existing heuristics.

**_Set a policy for limit enforcement_**

This flag allows the user to control what the JVM shall do if it does consume more native memory than its limit. 

    -XX:NativeMemoryLimitPolicy=none

If there is no native memory limit set, the default value is `none`.

    -XX:NativeMemoryLimitPolicy=warn

If the memory limit is set, the default value is `warn`. In this policy, the JVM will output a warning whenever consumption goes above the limit. 

    -XX:NativeMemoryLimitPolicy=enforced

If the limit is set and the policy is `enforced`, and the consumption goes above the limit, the JVM must exit with an `OutOfMemoryError`.

**_Valid native memory limits_**

The limit must be `-1` (no limit applied) or greater than or equal to `4MB`, which is the current limit of the root chunk size in the Metaspace. Otherwise, the JVM launcher must fail with _Invalid native memory limit_.

There is no effect if the policy is `enforced` or `warn`, and the limit is `-1`.

**_Native memory safe margin_**

A safe margin of a minimum of `4MB` and up to `2%` of the total memory expected by the JVM, with a maximum of `256MB`, shall be subtracted from the total amount of available memory, and the result be observed by the JVM as the hard environment memory limit. This is to account for any other memory consumption, and to avoid the JVM from exiting with an `OutOfMemoryError` too soon.

The safe magin may be manually configured, in absolute amount, with the following flag:

    -XX:NativeMemorySafeMargin=<input>

This value is only used if a valid Native Memory Limit is set and auto heap sizing is enabled.

**_Auto heap sizing_**

If the flag `-XX:+UseAutoHeapSizing` is set along with a valid Native Memory limit, the heap will be calculated automatically based on the total amount of available memory, minus the native memory limit and a safe margin.

**_Reading the native memory limit_**

The limit may be read through the `java.lang.management.MemoryMXBean` object:

```java
var memBean = ManagementFactory.getMemoryMXBean() ;
var nonHeap = memBean.getNonHeapMemoryUsage();
var limit = var.getMax();
```

**_Estimate native memory consumption_**

Combined with `-XX:NativeMemoryTracking` at development/test phase -- developers may identify a reasonable value as a starting point, and it shall be more straightforward to do resource planning and memory allocation to JVM processes in production upon first-time deployments.

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
