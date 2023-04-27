Summary
-------

Introduce ergonomics profiles and add a `balanced` option, for the existing heuristics, and a `dedicated` option for when the JVM is running on systems with dedicated resources for the one JVM process.

Goals
-----

1. Introduce the concept of ergonomics profiles.
1. Introduce a new flag to select an ergonomic profile.
1. Define existing heuristics and ergonomics as `balanced`.
1. Introduce a `dedicated` profile designed for systems where the JVM is effectively the only process using resources, e.g. In a canonical container deployment scenario.
1. Automatically select `dedicated` when the JVM believes the system is dedicated to it.

Non-Goals
---------

It is not a goal of this JEP to ensure applications' performance will be improved when running under the `dedicated` profile.

Success Metrics
---------------

No specific success metrics needed.

Motivation
----------

The default JVM ergonomics were designed to be balanced, with the understanding that it would share resources with other processes (e.g., a data store) running on a shared environment, such as a bare metal server or a large virtual machine.

On a study done by an APM vendor (New Relic), it was identified that more than 60% of monitored JVMs in production are running inside environments with resources dedicated to the JVM (i.e., In containers), as opposed to being shared. A large percentage of those JVMs with dedicated resources were running without explicit JVM tuning flags. Therefore the JVM was running with default ergonomics that are traditionally aimed at shared environments.

With the existing 'shared' default ergonomics, memory and CPU resources are not fully utilized by the JVM and the system therefore ends up wasting resources. Users then resort to horizontal scaling to address performance issues, before addressing resource planning for the JVM (and its tuning). This in turn leads to even more resource waste.

By maximizing resource consumption on environments where there are dedicated resources for the JVM process, the JVM has more opportunities to improve throughput and latency, or at the least, meeting the resource consumption (footprint) expected by the user.

The default ergonomics at present times are:

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

It is possible to observe that these amounts don't adequately map to the intended resource plan of dedicated environments, where the user may have already considered to allocate e.g. 4GB of memory to the JVM, and expect it to use 4GB of memory. The JVM will instead use, in this example, 1GB of memory, and the user will have to manually tune the JVM to use the expected amount of memory.

Further on, it is likely that the JVM does not need to reclaim heap memory as soon as it does in shared environments, as the JVM is the only process running on the system. This means that the JVM may set the initial heap size the same as maximum heap size, while having a fair minimum heap size for other memory pools consumption (e.g. native memory). This shall hint to the garbage collector to delay, or act more lazily, on the action of cleaning and reclaiming heap.

**Garbage Collector**

The garbage collector selection happens only among two: Serial GC and G1GC, based on the amount of active processors seen by the JVM, and the amount of available memory, with a slightly different way of defining GC thread pool:

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

Which GC is used by the application will impact on the amount of native memory consumed by the JVM, as well as impact throughput and latency.

**CPU allocation**

The CPU allocation, in most systems, is about CPU time rather CPU count. This is especially true in container-based environments. The JVM will expose the number of _active processors_ throught the API `Runtime.getRuntime().availableProcessors()`, which in turn is used by the JVM itself to size internal thread pools, and then read by 3rd-party frameworks and libraries to size their own thread pools.

In systems where CPU time is managed with limits, the JVM will often be at risk of suffering from _CPU throttling_, and it must be careful to not exaggerate on the number of threads it creates, otherwise the application will be impacted even further.

CPU allocation is the most challenging aspect of ergonomics, as the consequences are unpredictable without knowing what workload the JVM is running, and what it will actually need. Currently, the JVM calculates the number of active processors with the following check:

* Server: 1 “active processor” for each CPU
* Cgroups-based counting on cpu_quota/cpu_period

The user may override the calculation with the parameter `-XX:ActiveProcessorCount=<n>`. In practice, developers tend to not use this, and will expect the JVM to size itself accordingly. But a JVM running with a single observed "active processor" may still run multiple threads in parallel. Although they will consume the allowed CPU time much faster, in some applications this is negligible, as the application may be having significant wait times on IO operations to finish.

Description
-----------

We propose to add the concept of Ergonomics Profiles, while naming the existing default ergonomics as `balanced`, and adding a second profile called `dedicated` for when the JVM is to run under environments with resources dedicated to it. New profiles may be added in the future.

**_Selecting a profile_**

To select a profile, we propose a new flag:

    -XX:ErgonomicsProfile=<balanced|dedicated>

Users may also select a profile by setting this flag in the environment variable `JAVA_TOOL_OPTIONS`.

**_Default profile selection_**

The `balanced` ergonomics profile will be selected by default. 

If the JVM beleives it is running in a dedicated environment (e.g., containers) then the `dedicated` profile will be activated.

**_Balanced profile_**

The `balanced` profile is what the HotSpot JVM does today in terms of default ergonomics.

**_Dedicated profile_**

The `dedicated` profile will contain different heuristics aimed at maximizing resource consumption in the environment with the assumption that the environment is dedicated to the JVM.

This profile will maximize heap size allocation, optimize garbage collector selection with a larger set of options and considerations, vary the active processor counting, set different values for garbage collector threads, more aggressively size native memory expectation, and size internal JVM thread pools accordingly.

The table below describes what the `dedicated` profile will set for the JVM:

* GC selection: TBD
* Default heap size: TBD
* Active processor counting: TBD
* GC threads: TBD
* Thread pools: TBD

**_Identify selected profile_**

_Option 1_

The profile selection may be obtained programmatically by reading the property `java.vm.ergonomics.profile`:

```java
var ergonomicsProfile = System.getProperty("java.vm.ergonomics.profile");
```

_Option 2_

The profile selection may be obtained programmatically by the inclusion of the following method in `java.management.RuntimeMXBean`:

    String getJvmErgonomicsProfile()

To allow the backporting of this feature, the value may also be obtained through the `MBeanServerConnection.getAttribute` method:

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

// Describe any risks or assumptions that must be considered along with
// this proposal.  Could any plausible events derail this work, or even
// render it unnecessary?  If you have mitigation plans for the known
// risks then please describe them.

Dependencies
------------

No dependencies identified at the moment.
