# Ergonomics Profiles

Ongoing PR: https://github.com/microsoft/openjdk-jdk/pull/9

## Summary

Introduce a new JVM feature named Ergonomics Profiles, with a `shared` profile for the existing heuristics and a `dedicated` option for when the JVM is running on systems with dedicated resources for the one JVM process.

## Goals

Reduce resource waste on dedicated environmedns while preserving legacy heuristics by:

1. Introducing the concept of ergonomics profiles.
1. Remove code paths, flags, and checks related to Server and Client class machine
1. Define existing heuristics as `shared`, and maintaining it as default.
1. Define a new set of heuristics designed for environments where the JVM is expected to be the dominant process, e.g., in a container.
1. Define an `auto` mode to automatically select between `dedicated` and `shared` based on environment characteristics.

## Non-Goals

 - It is not a goal to guarantee applications' performance improvements when running under the `dedicated` profile.
 - It is not a goal to automatically change to the `dedicated` profile in environments such as containers.
 - It is not a goal to remove the configurability of any tuning flags such as heap sizes, GC selection, and others.

## Success Metrics

The `dedicated` profile, when active, should produce lesser resource waste with no performance regression, for a majority of workloads, compared to traditional heuristics, when running in a dedicated environment. 

## Motivation

The original design of default JVM ergonomics and heuristics was aimed at traditional bare metal servers or large virtual machine environments where the JVM shares resources with other processes (e.g., a data store). Today, more than half of Cloud based JVM workloads are running in dedicated environments such as containers, followed by a second majority of workloads on virtual machines with a percentage of those being exclusively dedicated to the JVM.

A study in 2023 by New Relic, an APM vendor with access to millions of JVMs in production, identified that more than 70% of their customers' JVMs were running inside dedicated environments. Many of these JVMs were running without explicit JVM tuning flags. Therefore the JVM was running with default ergonomics traditionally aimed at shared environments. Under this condition, the JVM does not utilize most of the memory available. With underuse of available resources, application developers and operators tend to apply horizontal scaling to address performance issues. This premature move to horizontal scaling, in turn, leads to more resource waste, both in terms of computing resources and engineering resources.

By increasing JVM's awareness to more resources available in dedicated environments when running without specific configurations other than the `dedicated` ergonomic profile selection, the JVM will have more opportunities to behave adequately, or at the very least meet the resource consumption (footprint) expected by the user. 

## Description

This JEP proposes adding the concept of Ergonomics Profiles. The existing ergonomics will be placed under the `shared` profile. A second profile named `dedicated` is designed for when the JVM is aimed at environments with dedicated resources, such as, but not limited to, Linux containers. A third value, `auto`, may be used to let the JVM select the ergonomic profile based on environment characteristics (e.g. it's running inside a container). The default profile will be `shared`.

### Selecting a profile

To set an ergonomic profile, the user may provide the following flag:

```sh
-XX:ErgonomicsProfile=<shared|dedicated|auto>
```

### Default profile

The `shared` ergonomics profile will be selected by default.

### Auto profile selection

When the the `auto` mode is selected, the JVM will activate the `dedicated` profile in cases it believes the resources are dedicated. This JEP proposes support for Linux containers dectection only.

### Shared profile

The `shared` profile represents the heuristics that the HotSpot JVM uses today.

### Dedicated profile

Assuming the environment is dedicated to the JVM, the `dedicated` profile will contain different heuristics to minimize resource waste. Two primary characteristics will be adjusted: heap size (initial and maximum) and the garbage collector selected. Heap size allocation will be less conservative compared to `shared`, and garbage collector selection will consider more options. The table below describes what the `dedicated` profile will set for the JVM:

* GC selection: see below
* Default maximum heap size: see below
* Default initial heap size: 50%
* Default minimum heap size: unchanged
* GC threads: unchanged

#### Selecting a garbage collector in dedicated profile

| Memory     | Processors | GC selected |
| ---------- | ---------- | ----------- |
| Any        | 1          | Serial      |
| \<=2048 MB | \>1        | Parallel    |
| \>2048 MB  | \>1        | G1          |

#### Default initial heap size

The `dedicated` profile will set an `InitialRAMPercentage` at 50%.

#### Default maximum heap size

The heap size percentage progressively grows based on the available memory to reduce waste on memory reserved for non-heap operations. This heap size percentage growth is based on common workloads observed in containers, such as REST-based web applications and microservices. If the percentage were to be the same, progressive waste would still occur.

| Memory      | Heap size |
| ----------- | --------- |
| <  0.5 GB   | 50%       |
| >= 0.5 GB   | 75%       |
| >= 4   GB   | 80%       |
| >= 6   GB   | 85%       |
| >= 16  GB   | 90%       |

### Changes to RuntimeMXBean

An application can obtain the profile selection programmatically by reading the property `java.vm.ergonomics.profile`:

```java
var ergonomicsProfile = System.getProperty("java.vm.ergonomics.profile");
```

The profile selection may also be obtained programmatically through JMX by the inclusion of the following method in `java.management.RuntimeMXBean`:

```java
String getJvmErgonomicsProfile()
```

### Deprecation of Server and Client Class Machine model

The HotSpot JVM has a concept of Server and Client Class machine model. This concept will be deprected, along with the following flags:

- `-XX:+NeverActAsServerClassMachine`
- `-XX:+AlwaysActAsServerClassMachine`

A warning message will advert the user that these flags will be removed in the future. To achieve the same traditional heuristics as `Client Class Machine`, users will have to ensure the `shared` (default) profile is selected.

## Testing

This proposal only affects ergonomics heuristics for when `dedicated` profile is active, by either selecting it directly, or by using `auto` mode.
This profile will be thoroughly performance tested with a variety of workloads commonly deployed in dedicated environments such as containers, with a variety of memory and CPU settings.

## Risks and Assumptions
The `dedicated` profile may not be ideal for certain workloads where non-heap memory is required beyond assumed differences between total available memory and heuristics ergonomically selected.
For these workloads, manual tuning will still be required.

---- END OF JEP ----

This section is only for documentation purposes.

## Current Ergonomics

Currently, the default ergonomics of the HotSpot JVM are:

### Memory Allocation

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

We can observe that these amounts don't adequately map to the intended resource plan of dedicated environments. The user may have already considered allocating, e.g., 4GB of memory to the environment and expect the JVM to use _nearly_ 4GB of memory. In this example, the JVM will instead use only 1GB of memory by default (25%) for its heap, and the user will have to manually tune the JVM if they want to ensure larger heap.

Furthermore, it is likely that the JVM can reclaim heap memory later than it does in shared environments, as the JVM is the only process running on such environment. Knowing it is the only or primary process, this means that the JVM may set the initial heap size closer to the maximum heap size while having a suitable minimum heap size for other memory pools (e.g., native memory).

### Garbage Collector

The default garbage collector selection happens only among two: Serial GC and G1 GC, based on the number of active processors seen by the JVM, and the amount of available memory, with a slightly different way of defining the GC thread pool:

**_GC selection_**

| Memory     | Processors | GC selected |
| ---------- | ---------- | ----------- |
| \>=1792 MB | \>1        | G1          |
| Otherwise  |            | Serial      |

Which GC is used by the application will impact the amount of native memory consumed by the JVM, and the throughput and latency of the application.

_**GC threads**_

We document the current implementation detail of GC threads ergonomically configured by the JVM for reference.

| GC        | \# of CPUs | ConcGCThreads                   | ParallelGCThreads      |
| --------- | ---------- | ------------------------------- | ---------------------- |
| Serial GC | any        | 0                               | 0                      |
| G1 GC     | 1-8        | max((ParallelGCThreads+2)/4, 1) | ceil(#CPUs)            |
| G1 GC     | \>8        | max((ParallelGCThreads+2)/4, 1) | 8 + (#CPUs-8) \* (5/8) |

## Dependencies

List related issues, in no particular order:

1. [JDK-8329758: ZGC: Automatic Heap Sizing](https://bugs.openjdk.org/browse/JDK-8329758)
1. [JDK-8261242: \[Linux\] OSContainer::is_containerized() returns true when run outside a container](https://bugs.openjdk.org/browse/JDK-8261242)
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
1. [Memory Allocation Table](https://docs.google.com/spreadsheets/d/15OiJ46Xz-Xjdm7brALqHgjdbkLd-C8ghH3bqKMuD9TQ/edit?usp=sharing)
