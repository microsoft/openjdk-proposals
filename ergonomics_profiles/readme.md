Summary
-------

Introduce a new JVM feature for Ergonomics Profiles, with a `shared` profile for the existing heuristics and a `dedicated` option for when the JVM is running on systems with dedicated resources for the one JVM process.

Goals
-----

1. Introduce the concept of ergonomics profiles.
1. Introduce a new flag to select an ergonomic profile.
1. Define existing heuristics and ergonomics as `shared`.
1. Introduce a `dedicated` profile designed for systems where the JVM is effectively the only process using resources, e.g. In a canonical container deployment scenario.
1. Automatically select the `dedicated` profile when the JVM believes system resources are dedicated to it.

Non-Goals
---------

This JEP does not aim to ensure applications' performance will be improved when running under the `dedicated` profile.

Success Metrics
---------------

No specific success metrics are needed.

Motivation
----------

The default JVM ergonomics were designed to be balanced with shared resources, with the understanding that it would share resources with other processes (e.g., a data store) running on a shared environment, such as a bare metal server or a large virtual machine.

A study done by an APM vendor (New Relic) identified that more than 60% of monitored JVMs in production are running inside environments with resources dedicated to the JVM (i.e., In containers) instead of being shared. Many of those JVMs with dedicated resources were running without explicit JVM tuning flags. Therefore the JVM was running with default ergonomics that are traditionally aimed at shared environments.

With the existing 'shared' default ergonomics, the JVM does not fully utilize memory and CPU resources, and the system wastes resources. Users then resort to horizontal scaling to address performance issues before addressing resource planning for the JVM (and its tuning). This, in turn, leads to even more resource waste.

By maximizing resource consumption in environments with dedicated resources for the JVM process, the JVM has more opportunities to improve throughput and latency, or at the least, meet the resource consumption (footprint) expected by the user.

Currently, the default ergonomics are:

**Memory Allocation**

The default maximum heap size of the JVM varies from 50% to 25%, depending on how much memory is available in the environment. The table below describes the current heuristics of OpenJDK 21:

**_Max heap size_**

| Available memory | Default |
|------------------|---------|
| < 256 MB         | 50%     |
| 256 - 512 MB     | ~126 MB |
| >= 512 MB        | 25%     |

The initial heap size also varies. The table below describes the current heuristics of OpenJDK 21:

**_Initial heap size_**

| Available memory | Default      |
|------------------|--------------|
| <= 512 MB        | 8 MB         |
| >= 512 MB        | 1.5625%-1.7% |

**_Min heap size_**

| Available memory | Default      |
|------------------|--------------|
| 64 MB – 8192 MB  | 8 MB         |

It is possible to observe that these amounts don't adequately map to the intended resource plan of dedicated environments. The user may have already considered allocating, e.g., 4GB of memory to the JVM and expect it to use 4GB of memory. In this example, the JVM will instead use 1GB of memory, and the user will have to manually tune the JVM to use the expected amount of memory.

Further on, it is likely that the JVM can reclaim heap memory later than it does in shared environments, as the JVM is the only process running on the system. This means that the JVM may set the initial heap size as maximum heap size, while having a suitable minimum heap size for other memory pools consumption (e.g. native memory). This shall signal to the garbage collector to delay, or act more lazily, on the action of cleaning and reclaiming heap.

**Garbage Collector**

The garbage collector selection happens only among two: Serial GC and G1GC, based on the number of active processors seen by the JVM, and the amount of available memory, with a slightly different way of defining the GC thread pool:

**_GC selection_**

| Memory     | Processors | GC selected |
| ---------- | ---------- | ----------- |
| \>=1792 MB | \>1        | G1          |
| Otherwise  |            | Serial      |

_**GC threads**_

| GC        | \# of CPUs | ConcGCThreads                   | ParallelGCThreads      |
| --------- | ---------- | ------------------------------- | ---------------------- |
| Serial GC | any        | 0                               | 0                      |
| G1 GC     | 1-8        | max((ParallelGCThreads+2)/4, 1) | ceil(#CPUs)            |
| G1 GC     | \>8        | max((ParallelGCThreads+2)/4, 1) | 8 + (#CPUs-8) \* (5/8) |

Which GC is used by the application will impact the amount of native memory consumed by the JVM, as well as impact throughput and latency.

**CPU allocation**

The CPU allocation, in most systems, is about CPU time instead of the CPU count. This is especially true in container-based environments. The JVM will expose the number of _active processors_ through the API `Runtime.getRuntime().availableProcessors()`, which in turn is used by the JVM itself to size internal thread pools and then read by 3rd-party frameworks and libraries to size their thread pools.

In systems where CPU time is managed with limits, the JVM will often be at risk of suffering from _CPU throttling_, and it must be careful to not exaggerate on the number of threads it creates, otherwise the application will be impacted even further.

CPU allocation is the most challenging aspect of ergonomics, as the consequences are unpredictable without knowing what workload the JVM is running and what it will actually need. Currently, the JVM calculates the number of active processors with the following check:

* Server: 1 “active processor” for each CPU
* Cgroups-based counting on cpu_quota/cpu_period

The user may override the calculation with the parameter `-XX:ActiveProcessorCount=<n>`. In practice, developers tend to not use this, and will expect the JVM to size itself accordingly. But a JVM running with a single observed "active processor" may still run multiple threads in parallel. Although they will consume the allowed CPU time much faster, in some applications this is negligible, as the application may be having significant wait times on IO operations to finish.

Description
-----------

We propose adding the concept of Ergonomics Profiles while naming the existing default ergonomics as `shared`, and adding a second profile called `dedicated` for when the JVM is to run under environments with resources dedicated to it. New profiles may be added in the future.

**_Selecting a profile_**

To select a profile, we propose a new flag:

    -XX:ErgonomicsProfile=<shared|dedicated>

Users may also select a profile by setting this flag in the environment variable `JAVA_TOOL_OPTIONS`.

**_Default profile selection_**

The `shared` ergonomics profile will be selected by default. 

If the JVM beleives it is running in a dedicated environment (e.g., containers) then the `dedicated` profile will be activated.

**_Shared profile_**

The `shared` profile is what the HotSpot JVM does today in terms of default ergonomics.

**_Dedicated profile_**

The `dedicated` profile will contain different heuristics aimed at maximizing resource consumption in the environment, assuming that the environment is dedicated to the JVM.

This profile will maximize heap size allocation, optimize garbage collector selection with a larger set of options and considerations, vary the active processor counting, set different values for garbage collector threads, more aggressively size native memory expectation, and size internal JVM thread pools accordingly.

The table below describes what the `dedicated` profile will set for the JVM:

* GC selection: TBD
* Default heap size: TBD
* Active processor counting: TBD
* GC threads: TBD
* Thread pools: TBD

**_Identify selected profile_**

The profile selection may be obtained programmatically by reading the property `java.vm.ergonomics.profile`:

```java
var ergonomicsProfile = System.getProperty("java.vm.ergonomics.profile");
```

The profile selection may also be obtained programmatically through JMX by the inclusion of the following method in `java.management.RuntimeMXBean`:

    String getJvmErgonomicsProfile()

To allow the backporting of this feature to older versions of OpenJDK, the value may also be obtained through the `MBeanServerConnection.getAttribute` method:

```java
MBeanServerConnection mbs = ...

var oname = new ObjectName(ManagementFactory.RUNTIME_MXBEAN_NAME);
var ergonomicsProfile = mbs.getAttribute(oname, "JvmErgonomicsProfile");
```

Alternatives
------------

At this moment, the only alternative to this proposal is by manualling tuning the JVM. This exercise is very often complex, difficult, and time consuming for Java developers.

Testing
-------

// What kinds of test development and execution will be required in order
// to validate this enhancement, beyond the usual mandatory unit tests?
// Be sure to list any special platform or hardware requirements.

Risks and Assumptions
---------------------

None at this time.

Dependencies
------------

List of related issues, in no particular order, that may have to be addressed before this JEP, or that this JEP may have address as part of its implementation:

1. [JDK-8261242: [Linux] OSContainer::is_containerized() returns true when run outside a container](https://bugs.openjdk.org/browse/JDK-8261242)
1. [JDK-8302744: Refactor Hotspot container detection code](https://bugs.openjdk.org/browse/JDK-8302744)
1. [JDK-8264482: container info misleads on non-container environment](https://bugs.openjdk.org/browse/JDK-8264482)
1. [JDK-8285277: Improve container support for memory limits](https://bugs.openjdk.org/browse/JDK-8285277)
1. [JDK-8292742: Remove container support method cpu_share()](https://bugs.openjdk.org/browse/JDK-8292742)
1. [JDK-8284900: Check InitialHeapSize and container memory limits before startup](https://bugs.openjdk.org/browse/JDK-8284900)
1. [JDK-8278492: Confusion about parameter -XX:MinRAMPercentage](https://bugs.openjdk.org/browse/JDK-8278492)
1. [JDK-8287185: Consolidate HotSpot and JDK tests for CgroupSubsystemFactory](https://bugs.openjdk.org/browse/JDK-8287185)
1. [JDK-8078703: Ensure that GC's use of processor count is correct](https://bugs.openjdk.org/browse/JDK-8078703)
1. [JDK-8286991: Hotspot container subsystem unaware of VM moving cgroups](https://bugs.openjdk.org/browse/JDK-8286991)
1. [JDK-8286212: Cgroup v1 initialization causes NPE on some systems](https://bugs.openjdk.org/browse/JDK-8286212)
