Summary
-------

Introduce ergonomics profiles with a `balanced` option, and an `dedicated` option of default ergonomics for when the JVM is running on systems with dedicated resources for the one JVM process.

Goals
-----

1. Introduce the concept of ergonomic profiles.
1. Introduce a new flag to select an ergonomic profile.
1. Define existing heuristics and ergonomics as `balanced`.
1. Introduce an `dedicated` profile designed for systems with dedicated resources not to be shared with other processes.
1. Automatically select `dedicated` when the JVM believes the system is dedicated to it.

Non-Goals
---------

It is not a goal of this JEP to ensure applications' performance will either improve or degrade when running under the `dedicated` profile.

Success Metrics
---------------

No specific success metrics needed.

Motivation
----------

The default ergonomics were designed to be balanced, and fair with other processes running on a shared environment, such as a bare metal server or a large virtual machine. On a study done by an APM vendor (New Relic), it was identified that more than 60% of monitored JVMs in production are running inside environments with dedicated resources. A large set of those were running without explicit JVM tuning flags, and therefore the JVM was running with default ergonomics.

With default ergonomics, much of the resources (memory and CPUs) intended for the JVM are not fully in use and the system ends up wasting resources. Users then, resort to horizontal scaling to address performance issues, to which in turn leads to even more resource waste.

By maximizing resource allocation on environments where it is believed to have dedicated resources to the one JVM process, the JVM has increasing chances of performing better, or at the very least meeting resource consumption as expected by the user, especially when not manually tunned.

**Memory allocation**

The default maximum heap size of the JVM varies from 50% to 25%, depending on how much memory is available in the environment. The table below describes the current heuristics of OpenJDK 21:

**_Max heap size_**

| Available memory | Default |
|------------------|---------|
| < 256 MB         | 50%     |
| 256 - 512 MB     | ~126 MB |
| >= 512 MB        | 25%     |

**_Initial heap size_**

| Available memory | Default      |
|------------------|--------------|
| <= 512 MB        | 8 MB         |
| >= 512 MB        | 1.5625%-1.7% |

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

The CPU allocation, in most systems, is about CPU time rather CPU count. The JVM will expose the number of _active processors_ throught he API `Runtime.getRuntime().availableProcessors()`, which in turn is used by the JVM itself to size internal thread pools, and then read by 3rd-party frameworks and libraries to size their own thread pools.

In systems where CPU time is managed with limits, the JVM will often suffer from _CPU throttling_, and must be careful to not exaggerate on the number of threads it creates, otherwise the application suffers even further.

CPU allocation is the most challenging aspect of ergonomics, as the consequences are unpredictable without knowing what the workload the JVM is running, will actually need. Currently, the JVM calculates the number of active processors with the following check:

* Server: 1 “active processor” for each CPU
* Cgroups-based counting on cpu_quota/cpu_period

The user may override the calculation with the parameter `-XX:ActiveProcessorCount=<n>`. In practice, developers tend to not use this, and will expect the JVM to size itself accordingly. But a JVM running with a single observed "active processor" may still run multiple threads in parallel, although they will consume the allowed CPU time much faster. In some applications, this is negligible, as the application may be having significant wait times on IO operations to finish.

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

Alternatives
------------

// Did you consider any alternative approaches or technologies?  If so
// then please describe them here and explain why they were not chosen.

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

// Describe all dependencies that this JEP has on other JEPs, JBS issues,
// components, products, or anything else.  Dependencies upon JEPs or JBS
// issues should also be recorded as links in the JEP issue itself.
//
// Describe any JEPs that depend upon this JEP, and likewise make sure
// they are linked to this issue in JBS.
