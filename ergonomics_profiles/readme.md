Summary
-------

Introduce ergonomics profiles and add a `balanced` option, for the existing heuristics, and a `dedicated` option for when the JVM is running on systems with dedicated resources for the one JVM process.

Goals
-----

1. Introduce the concept of ergonomics profiles.
1. Introduce a new flag to select an ergonomic profile.
1. Define existing heuristics and ergonomics as `balanced`.
1. Introduce a `dedicated` profile designed for systems with dedicated resources not expected to be shared with other processes.
1. Automatically select `dedicated` when the JVM believes the system is dedicated to it.

Non-Goals
---------

It is not a goal of this JEP to ensure applications' performance will be significantly improved when running under the `dedicated` profile.

Success Metrics
---------------

No specific success metrics needed.

Motivation
----------

The default ergonomics were designed to be balanced, and fair with other processes running on a shared environment, such as a bare metal server or a large virtual machine. On a study done by an APM vendor (New Relic), it was identified that more than 60% of monitored JVMs in production are running inside environments with dedicated resources such as containers. A large set of those were running without explicit JVM tuning flags, and therefore the JVM was running with default ergonomics, traditionally aimed at shared environments.

With these existing default ergonomics, much of the resources (memory and CPUs) intended for the JVM in a dedicated environment, are not fully in use and the system ends up wasting resources. Users then, resort to horizontal scaling to address performance issues even before addressing resource planning for the JVM and its tuning, to which in turn leads to even more resource waste.

By maximizing resource consumption on environments where it is believed to have dedicated resources to the one JVM process, the JVM thus has increasing chances of performing better, or at the very least meeting resource consumption as expected by the user, especially when not manually tunned.

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

Further on, it is likely that the JVM does not need to reclaim heap memory as often as it does in shared environments, as the JVM is the only process running on the system. This means that the JVM may set the initial and minimum heap size the same as maximum heap size. This shall hint to the garbage collector to delay, or act more lazily, on the action of cleaning and reclaiming heap.

**_Native Memory_**

With the inclusion of Elastic Metaspace (JEP 387), it is safer to assume that Native Memory consumption will have reduced growth during the course of execution of a Java application. While the selection of a Garbage Collector certainly impacts Native Memory consumption, GC tuning is an exercise that is best left to the user, as it is highly dependent on the application and its workload. The JVM will still select a GC based on heuristics, and should take into consideration that native memory consumption will happen, and account for that.

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

We propose to add a new ergonomics mode, called `dedicated`, which will be the default mode for the JVM under certain conditions, while turned off for most traditional environments. This mode will be activated by the presence of the flag `-XX:+UseDedicatedErgonomics`.

**_Selecting a profile_**

This JEP proposes, at this moment, only two profiles: `balanced` and `dedicated`. New profiles may be added in the future.

The `dedicated` ergonomics profile may be turned on with the flag `-XX:+UseDedicatedErgonomics`. It will be turned on by default if the JVM believes it is running in a dedicated environment (e.g. containers). 

The `balanced` ergonomics profile is the default profile, and it is what the HotSpot JVM offers today. It will be the default profile for the JVM when the flag `-XX:+UseDedicatedErgonomics` is not present.

The flag `-XX:+UseBalancedErgonomics` will be added to allow the user to explicitly select the `balanced` profile. The first occurrence of an ergonomics profile selection will be the one that is used, and the JVM will ignore subsequent profile selection flags.

The presence of the environment variable `JVM_ERGONOMICS_PROFILE` may also select the profile. The JVM will check for the presence of this environment variable, and if it is present, it will use the value of the variable to select the profile. The value of the variable must be one of the following: `balanced`, `dedicated`. If the value is not one of the above, the JVM will ignore the environment variable and use a default profile selection. This environment variable overrides an explicit JVM ergonomics profile selection done with a JVM flag.

**_Balanced profile_**

The balanced is what the HotSpot JVM does today.

**_Dedicated profile_**

The dedicated profile will contain different heuristics aimed at maximizing resource consumption in the environment with the assumption that the environment is dedicated to the JVM.

This profile will maximize heap size allocation, garbage collector selection, active processor counting, garbage collector threads, native memory sizing, and other JVM thread pools.

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
