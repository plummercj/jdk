/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
package org.openjdk.bench.vm.compiler;

import org.openjdk.jmh.annotations.*;

import java.lang.invoke.*;
import java.util.concurrent.TimeUnit;

/*
  We had to disable the optimization that moves the dst load of an arraycopy
  to be a src load. Here, we show the performance impact only affects a
  middle range, somewhere between 8-64 elements.
 */

@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@State(Scope.Thread)
@Warmup(iterations = 2, time = 1000, timeUnit = TimeUnit.MILLISECONDS)
@Measurement(iterations = 5, time = 1000, timeUnit = TimeUnit.MILLISECONDS)
@Fork(value = 1)
public class ArrayCopyElimination {
    @Param({"4,  4", "8,  4", "16,  4", "32,  4", "64,  4", "128,  4",
                     "8,  8", "16,  8", "32,  8", "64,  8", "128,  8",
                              "16, 16", "32, 16", "64, 16", "128, 16",
                                        "32, 32", "64, 32", "128, 32",
                                                  "64, 64", "128, 64",
                                                            "128,128"})
    public String SIZE_PAIR;

    // To get compile-time constants for ALLOCATION_SIZE and COPY_SIZE
    static final MutableCallSite MUTABLE_CONSTANT_ALLOCATION_SIZE = new MutableCallSite(MethodType.methodType(int.class));
    static final MethodHandle MUTABLE_CONSTANT_ALLOCATION_SIZE_HANDLE = MUTABLE_CONSTANT_ALLOCATION_SIZE.dynamicInvoker();
    static final MutableCallSite MUTABLE_CONSTANT_COPY_SIZE = new MutableCallSite(MethodType.methodType(int.class));
    static final MethodHandle MUTABLE_CONSTANT_COPY_SIZE_HANDLE = MUTABLE_CONSTANT_COPY_SIZE.dynamicInvoker();

    private int[] srcI;

    @Setup
    public void init() throws Throwable {
        String[] parts = SIZE_PAIR.split(",");
        if (parts.length != 2) {
            throw new RuntimeException("SIZE_PAIR should be a comma-separated int pair");
        }
        int allocationSize = Integer.parseInt(parts[0].trim());
        int copySize = Integer.parseInt(parts[1].trim());
        if (allocationSize < copySize) {
            throw new RuntimeException("allocation size cannot be smaller than copy size");
        }

        MUTABLE_CONSTANT_ALLOCATION_SIZE.setTarget(MethodHandles.constant(int.class, allocationSize));
        MUTABLE_CONSTANT_COPY_SIZE.setTarget(MethodHandles.constant(int.class, copySize));

        srcI = new int[allocationSize];
    }

    @CompilerControl(CompilerControl.Mode.INLINE)
    private int allocation_size_con() throws Throwable {
        return (int) MUTABLE_CONSTANT_ALLOCATION_SIZE_HANDLE.invokeExact();
    }

    @CompilerControl(CompilerControl.Mode.INLINE)
    private int copy_size_con() throws Throwable {
        return (int) MUTABLE_CONSTANT_COPY_SIZE_HANDLE.invokeExact();
    }

    @Benchmark
    public int bench() throws Throwable {
        int allocationSize = allocation_size_con();
        int copySize = copy_size_con();
        // Note: the sizes are compile-time constants, so that array optimization
        //       can take place for the right sizes.
        int[] dst = new int[allocationSize];
        System.arraycopy(srcI, 0, dst, 0, copySize);
        return dst[copySize - 1];
    }
}
