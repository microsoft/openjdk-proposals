Summary
-------
 
Provide Stack Allocation support in HotSpot C2 to eliminate non-escaping heap
allocations where applicable. Stack Allocation support will be built on top 
of the existing escape analysis support and allocation elimination in C2.

Goals
-----
The proposal has the following goals: 
1. Enhance C2’s ability to remove heap allocations that are proven to be 
   non-escaping by the escape analysis optimization pass, and by that, reduce 
   heap pressure and CPU data cache misses.
2. To not regress performance on existing workloads.
3. Additional optimization passes that are added should have minimal impact on 
   compilation times, when measured on startup and ramp-up sensitive workloads.
4. Additional optimization passes that are added should have minimal additional 
   memory overhead during compilation of methods.
5. Reduce the number of instructions generated for object allocation by 
   removing the slow path. This will reduce the overall code cache size.
6. The optimization should have minimal memory footprint increase on debug 
   information used for deoptimization.
7. The optimization should work in all GC modes supported by the current 
   OpenJDK release.
8. The optimization should work with Project Loom when it becomes available.

Non-Goals
---------
1. This proposal will not replace or supersede any existing allocation 
   elimination optimizations already present in the C2 compiler.
2. This proposal is not a replacement to speculative scalar replacement based
   on speculative escape analysis.
3. This proposal will not change how escape analysis works in C2, e.g. 
   introduce context sensitive escape analysis.


Success Metrics
---------------
Success of this work will be gauged by the following metrics:

1. Reduction of heap allocations of up to 15% in standard Java benchmark 
   suites, e.g. Renaissance benchmarks, Scala DaCapo, etc.
2. No direct throughput or latency performance regressions because of the code 
   changes introduced by the proposal on standard benchmark suites.
3. Less than 2% performance regression on compilation time as measured on 
   startup and warm-up sensitive scenarios.
4. Up to 10% instruction count reduction per allocation site in C2 generated 
   code.
5. No more than 2% increase in size of debug information registered for 
   deoptimization.
 
Motivation
----------
Stack allocation of objects will improve the efficiency of the JVM heap. The 
reduction in heap usage will lessen the impact of garbage collection on 
application SLAs. The importance of GC overhead is evident in the increasing 
popularity of concurrent collectors like zGC and Shenandoah. 
The optimization is generally applicable and will benefit a large number of 
Java workloads. Currently, in C2 compiled methods, there’s a large discrepancy 
in number of objects that are identified as non-escaping and those that are 
scalar replaced. This optimization tries to fill in that gap.

Alternative VM implementations (like J9) already do this optimization, the 
optimization is planned to be implemented in .NET Core 
(https://github.com/dotnet/runtime/blob/master/docs/design/coreclr/jit/object-stack-allocation.md).

Description
-----------
The proposed optimization replaces allocation of objects on the heap with 
allocation of the objects on the stack, when possible and deemed safe by 
escape analysis. The stack memory is automatically reclaimed on method exit 
and therefore there isn’t any GC overhead in reclaiming the memory of stack 
allocated objects. This optimization is very similar to the existing scalar 
replacement optimization in C2, where heap objects are broken down into their 
respective fields and are replaced with autos. One main difference between 
stack allocation and scalar replacement is that stack allocation retains the 
original object shape on the stack. Retaining the object shape allows for 
elimination of more heap allocations, specifically in code where the use of 
the allocated object requires the original object shape. One particular 
example where the object shape must be retained is at control-flow merge 
points, where depending on the context an object can be escaping or 
non-escaping. 

To implement stack allocation, we need to modify the C2 compiler, the GCs and 
some of the VM runtime interfaces:

- Escape analysis will have to be modified to add identification of objects 
  that are candidates for stack allocation, similar to the identification of 
  candidates for scalar replacement.
- Macro node replacement will have to be modified to attempt stack allocation 
  of objects that fail the existing scalar replacement allocation elimination.
- OOP map runtime code will need to be modified to handle oops that can 
  potentially be stack allocated oops.
- GC barriers will have to be modified to skip objects that are allocated on 
  the stack. It will be required to add heap range checks for all barriers that 
  can be reached by stack allocated objects in methods where stack allocation 
  is performed. Unlike scalar replacement where all GC barriers are simply 
  eliminated, in the case of stack allocated objects, some barriers will be 
  eliminated while some will have to be extended with the heap bounds check. 
  The required barrier bounds checks will have to be considered at barrier 
  expansion time.
- We would need a new Node type to represent the stack oop in the IdealGraph 
  IR. Our current prototype reused BoxLockNode, but a proper subclass will 
  need to be added to avoid some of the code duplication and supplementary 
  checks we had to add in a few places. It would likely need to be a proper
  MachNode.
- Heap iterators will need to be changed to understand and walk stack oops. 
  This is required for proper GC support during root set scanning. 
- The SafepointScalarObject node will have to be extended or sub-classed for 
  deoptimization support of stack allocated objects, similarly to how scalar 
  replaced objects are handled.
- Deoptimization will have to be augmented to understand and translate stack 
  allocated objects into heap objects at a deoptimization event. Stack 
  allocated objects will be translated each time during deoptimization and then 
  later matched into the live set of objects. Namely, since we support merge 
  of stack allocated and heap objects in the control flow, the information of 
  whether the merge point has stack or heap object at hand is run-time only. 
  Deoptimization will have to perform run-time resolution of the merged objects 
  and it’s inevitable that there could be some small amount of heap allocation 
  wasted at a deoptimization event.
- Verification performed at deoptimization time will have to be extended to 
  support verifying of stack allocated objects.
- Special consideration will have to be put into restricting some of the 
  optimizations of stack allocated objects pointing to other stack allocated 
  objects in compressed oops mode. Currently the Hotspot stack is compressed 
  in compressed oops mode and at merge points the encode and decode oop 
  operations will not be able to handle both stack and heap objects.
 
Alternatives
------------
An alternative approach to achieving the same optimization would be to use 
Zone based allocation on the heap. A zone would be a heap area with known, 
limited lifetime, which can be deallocated by the runtime when the scope is 
popped on method exit. We decided to not pursue this approach because of 
potential GC complexities, although this particular approach has some 
advantages. Namely, using zone-based allocation would eliminate the compressed 
oops restrictions as well as make Loom support easier. If this approach is 
chosen instead a lot of the concepts in this proposal can be reused.
 
Testing
-------
No special testing requirements required beyond the usual suite of tests and 
new unit tests written for the specific optimization in various GC and 
deoptimization stress modes.
 
Risks and Assumptions
---------------------
There are a few potential risks of implementing this proposal:

- Stack allocation by definition will reserve a chunk of the thread stacks for 
  placing the objects on them. This additional space needed for allocating the 
  objects may push the stack usage of the application higher than usual. To 
  mitigate this risk, if proven to become an issue, additional logic will be 
  required to carefully select candidates for stack allocation.
- To support control flow merge points for stack allocated objects, we had to 
  add range checks in certain write barriers that might be reachable by stack 
  allocated objects. These additional barrier checks are very quick, but it’s 
  possible that a barrier with this additional check might be slower and cause 
  a regression in specific cases. In case this is proven to be an issue, an 
  additional logic would have to be added to carefully select which objects 
  should be stack allocated.
- There might be additional memory overhead for tracking stack allocated 
  objects for deoptimization purposes. We need to carefully measure this 
  overhead, and if it’s proven to be an issue, we might need to redesign how 
  we register the stack allocated objects for deoptimization.
- To support Project Loom stack migrations, we may need to restrict stack 
  allocation of certain objects that cross method calls. This particular 
  issue will require special consideration when adding support for 
  Project Loom. 
 
Dependencies
-----------
We do not currently think there are any dependencies.

Appendix
----------
### Alternatives for using BoxLockNode as a placeholder for stack allocated objects

We currently re-used BoxLockNode in C2 to support the creation of a stack oop 
in the IdealGraph IR. While using BoxLockNode was a way forward to write the 
prototype code, we don’t think it is acceptable as a final solution because 
of the following reasons:

- We had to add some special if/else statements to differentiate the use of 
  the BoxLockNode for monitor and stack allocation purposes.
- There was a register assignment issue related to BoxLockNode that caused 
  registers to not be materialized in merge blocks during code generation 
  time. We had to unmark BoxLockNode as a node that doesn’t generate code to 
  allow the spill code to be correctly generated.
- The code generated for BoxLockNode seems very inefficient, frequently 
  re-materializing the stack location in a register. This creates a longer 
  code sequence than desirable.

### We reused SafepointScalarObjectNode, is this acceptable?
To support deoptimization of stack allocated object, we reused the 
SafepointScalarObjectNode that is currently used to heapify the eliminated 
allocations by scalar replacement. We reused this Node and added some 
additional flags and checks to differentiate the stack allocated objects from 
the scalar replaced objects. We are not sure if this is an acceptable solution 
or if we need to add another runtime node type to support stack allocated 
objects heapification.

### When dealing with stack allocated objects in loops we need a lifetime overlap check. At the moment this check is very pessimistic and does an exhaustive search, can we do better? 
By definition stack allocated objects reuse the memory location between loop 
iterations. Reusing the memory location can cause wrong application behavior 
if the next loop iteration uses the object from the previous loop iteration. 
There are subtle issues with equality operations using the object address, and 
with reading the object field values stored by a previous iteration while the 
object initialization remains inside the loop.

In order to retain correct program behavior, we run exhaustive search on all 
uses of objects allocated in a loop. If any of the uses lead back to the 
allocate node itself, we reject the allocation as a candidate for stack 
allocation. Essentially, we need to tell if the object allocation ever escapes 
the loop, including escaping into the next loop iteration. We are hoping for 
suggestions on how to make this check a bit more computationally efficient 
compared to what we have so far, or for a different check altogether. 
Typically, we could stack allocate an object inside a loop if the 
initialization of the object is eliminated, but the current limited check 
would avoid marking the object as a stack allocation candidate.

### ArrayCopy helper calls that do barriers in runtime prevent stack allocation. Can we improve this with new helpers or an alternative approach? 
Stack allocation of any reference arrays that have an ArrayCopy use with a 
runtime helper is not allowed. The runtime helpers will perform write barriers 
on the array and run into an issue with a stack allocated array object. One 
possible solution is to introduce a bounds check in the ArrayCopy barrier code, 
similar to the bounds check we added to the normal barriers. However, this 
approach might cause a regression in other places that do not use stack 
allocated objects. As an alternative we can generate a new set of ArrayCopy 
runtime helpers, which have bound checks in their write barriers, to be used 
by generated code with stack allocated objects.

### Stack allocation relies on lock elimination to happen. Can we strengthen this check? 
Various forms of locking can potentially cause issues with stack allocated 
objects, so we assume that stack allocated objects will never be locked. To 
avoid locks on stack allocated objects we rely that lock elimination will 
happen on the objects we are stack allocating. At the moment we disable stack 
allocation if lock elimination is disabled. We are wondering if there are some 
additional checks we need to implement to ensure stack allocation and lock 
elimination are in sync?

### Any failed Stack Allocated/Scalar Replaced Allocations forces all referenced SA to be skipped
Candidate allocations for stack allocation or scalar replacement are marked 
during escape analysis time. We also allow stack allocated objects or scalar 
replaced allocations to point to other stack allocated objects. At macro 
expansion time, when allocations are eliminated or stack allocated, we could 
potentially fail to eliminate an allocation or stack allocate because of 
various restrictions. Failing to stack allocate or eliminate an allocation may 
result in “escapement” of an object that was deemed stack allocatable. Namely, 
if a stack allocated candidate was pointed to by another stack allocation 
candidate, and that other stack allocation candidate failed to be stack 
allocated, then we have effectively created an escape of the candidate, since 
the object that pointed to it is now going to be allocated on the heap. 

Because of this restriction, we now disable stack allocation on all objects 
that were pointed to by a stack or scalar replaced allocation as soon as one 
stack allocation fails. An alternative approach would be to retain an object 
graph for each referenced allocation with point-to edges, such that we can 
precisely tell which objects should be constrained from stack allocation upon 
earlier stack allocation/scalar replacement failure.

### Stack allocation reduces the size of methods because allocation slow paths are removed, which can affect inlining thresholds and cause unexpected results.
One unexpected side-effect of stack allocation we found was the effect it has 
on inlining. The inlining threshold that decides if a method should be inlined 
or not based on the size of the callee (when compiled), may inline a method 
that was previously rejected if the callee has stack allocated some objects. 
Namely, since stack allocation removes the allocation slow path, similarly to 
scalar replacement, the effective method body after compilation is slightly 
smaller with each stack allocated allocation site. This shrinkage in 
compilation size may affect the inlining thresholds and cause different 
performance characteristics in the application indirectly. We noticed such 
behavior while running Renaissance Neo4J Analytics benchmark with JDK11.

### Write barrier stack allocation checks
To simplify modifying the write barriers we add stack allocation checks to all 
barriers when they are created. If no stack allocated objects ever reach a 
particular barrier we remove the stack allocation check late in barrier 
expansion. Would it be better to reverse this and only add the check to 
required barriers?

### Support for Shenandoah and zGC
To simplify the prototype, support has only been added for ParallelGC and G1GC. 
What concerns need to be handled to support Shenandoah and zGC? Are there any 
special considerations we need to be aware of for read barriers?

### Java class library changes
Certain Java coding patterns might expose opportunities for stack allocation 
of arrays. Namely, stack allocation is able to work with control-flow merge 
points and if the allocation size was known on at least one side of the control 
flow we could stack allocate on that side. Namely code patterns, like the one 
below, benefit from stack allocation:

```
if (size > 10) {
	return new Integer[size];
} else {
	return new Integer[10];
}
```

This kind of pattern seems common with default size of arrays in the internals 
of certain collection classes. However, we often find the code pattern in a 
form that works against stack allocation, with the use of the Math.max/Math.min 
methods which simplify the Java code. For example, let’s consider a more 
concise implementation for the default array size pattern:

```
return new Integer[Math.max(size, 10)];
```

The array size for the code above will be a variable result from Math.max, and 
thus make the array length unknown. Stack allocation will fail because of a 
non-constant array size node. To help with some common library usage scenarios 
we have changed the code in ArrayList.java and Matcher.java, but perhaps 
another optimization can be applied for the case in general, e.g. versioning 
the allocation automatically using a conditional. One potential avenue is to 
implement code duplication as described in this paper 
http://ssw.jku.at/General/Staff/Leopoldseder/DBDS_CGO18_Preprint.pdf.

### Discussion on performance results

#### Renaissance Benchmark Suite performance results (Averages)
Benchmark documentation and download instructions: https://renaissance.dev

Test configurations:

Baseline: ```-XX:+UseParallelGC -Xmx8G -Xms8G -XX:-UseCompressedOops```

Stack Alloc: ```-XX:+UnlockExperimentalVMOptions -XX:+UseStackAllocation 
-XX:+UseParallelGC -Xmx8G -Xms8G -XX:-UseCompressedOops```

Test Machine:

```Mac OS X 10.15.5 (19F101) Quad-Core Intel Core i7, 3.1 GHz, 16GB of RAM```

| Benchmark name   | Baseline Score(ms) | Stack Alloc Score(ms) | Baseline Alloc(GB) | Stack Alloc(GB) |
| ---------------- | ------------------:| ---------------------:| ------------------:| ---------------:|
| als              |             2577.6 |                2480.0 |               72.4 |            59.6 |
| naïve-bayes      |              463.6 |                 380.8 |               59.4 |            54.4 |
| neo4j-analytics  |             6519.8 |                1933.0 |              135.9 |            15.3 |
| page-rank        |             5477.4 |                5194.2 |               46.7 |            42.0 |
| scala-stm-bench7 |             1350.4 |                1253.6 |               92.9 |            88.0 |

The benchmark performance varies a lot sometimes, therefore some of the numbers 
above are not easily reproducible without a lot of runs to produce averages. 
In our analysis of the reasons for the benchmark variability, we determined 
that some of the variability is related to differences in profiling information 
and inlining from run to run. We have also observed reduction in heap 
allocation on the benchmarks below without statistically significant observable 
performance improvement:

| Benchmark name  | Baseline Alloc(GB) | Stack Alloc(GB) |
| --------------- | ------------------:| ---------------:|
| dec-tree        |              357.0 |           341.7 |
| scala-dotty     |               65.7 |            63.0 |
| finagle-chirper |              345.4 |           335.1 |
| finagle-http    |               55.7 |            53.0 |
| log-regression  |               65.7 |            62.8 |
| philosophers    |              106.6 |           103.6 |
| reactors        |               94.2 |            91.4 |

There are plenty more opportunities for a lot of bigger gains in the Scala 
workloads in the Renaissance benchmarks suite, however, to materialize those 
improvements would require some work on the C2 inliner. We have made a few 
prototype patches to the C2 inliner to catch some of the common Scala 
opportunities, namely around the iterator functions, but submitting those is 
outside the scope of the stack allocation work presented here. We intend to 
make a follow-up submission with the inlining work after we have tested those 
changes more on other workloads.  

The largest observed performance improvement in Neo4j is not solely because of 
stack allocation. Stack allocation reduces the code generated for the 
allocation path, therefore making the overall compiled method size smaller. 
Neo4j benefits from a specific inlining opportunity that is exposed by the 
reduction of the compiled method size. After inlining on a certain path, a 
lot of allocations in Neo4j are deemed non-escaping and are removed with a 
combination of scalar replacement and stack allocation.

The command line options supplied to Java were purposefully selected to make 
the optimization have the biggest impact possible. Compressed oops have a 
limitation at the current implementation stage related to missing support for 
compressing stack oops. The default G1 collector currently blocks certain 
escape analysis candidates because of an escaping runtime post barrier call. 
Improving escape analysis for the G1 collector, as well as support for the 
new low latency collectors, is in plan for the future.

#### Scala Benchmark Suite performance results (Averages)

Benchmark documentation and download instructions: http://www.scalabench.org/

Test configurations:

Baseline: ```-Xmx256 -Xms256m -XX:-UseCompressedOops```

Stack Alloc: ```-XX:+UnlockExperimentalVMOptions -XX:+UseStackAllocation 
-Xmx256 -Xms256m -XX:-UseCompressedOops```

Test Machine:

```Mac OS X 10.14.5 (19F101) Quad-Core Intel Core i9, 2.3 GHz, 16GB of RAM```

| Benchmark name | Baseline Score(ms) | Stack Alloc Score(ms) | Baseline Alloc(GB) | Stack Alloc(GB) |
| -------------- | ------------------:| ---------------------:| ------------------:| ---------------:|
| factorie       |            13653.2 |               13293.1 |              209.2 |           107.5 |
| h2             |             1137.7 |                1142.7 |               53.7 |            52.2 |
| jython         |             1057.3 |                1091.8 |               49.2 |            47.8 |
| kiami          |              257.6 |                 257.1 |               28.6 |            28.1 |
| sunflow        |              941.3 |                 932.7 |               65.0 |            63.0 |
| tmt            |             4178.3 |                3747.2 |              151.6 |            66.2 |


