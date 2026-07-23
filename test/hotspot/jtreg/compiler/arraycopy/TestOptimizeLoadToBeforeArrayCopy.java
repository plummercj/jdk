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

package compiler.arraycopy;

import java.util.Arrays;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;

import compiler.lib.ir_framework.*;
import compiler.lib.verify.*;
import static compiler.lib.generators.Generators.G;
import compiler.lib.generators.Generator;

/*
 * @test
 * @bug 8388047
 * @summary Tests for allowing loads to be optimized before arraycopy,
 *          and related elimination of arraycopy, and rematerialization
 *          of arrays at deoptimization points.
 * @library /test/lib /
 * @run driver/timeout=480 ${test.main.class}
 */

/**
 * The goal of this test is to find examples for loads that "skip" arraycopy,
 * see LoadNode::can_see_arraycopy_value. Currently, we had to restrict the
 * optimization to cases where the src array is thread local. The goal of
 * this test is to show where the optimizations still work, and where we now
 * have restrictions that might have performance impact.
 *
 * There are the following relevant VM flags that are relevant optimizations
 * for the tests below:
 * - ArrayCopyLoadStoreMaxElem (default: 8)
 *   Maximum length of an arraycopy that gets expanded to load/store pairs.
 * - EliminateAllocationArraySizeLimit (default: 64)
 *   Maximum array length for array allocation elimination.
 *
 * Given the flags, we test different allocation and arraycopy length
 * combinations.
 *
 * Below, we assume the default setting of the two flags, and analyze the impact
 * of the thread-locality-restriction for the source array, when trying to
 * optimize a dst load into a src load in LoadNode::can_see_arraycopy_value.
 *
 * I'm marking the tests that are affected by the restriction under
 * default flag values, with an [AFFECTED]. From the cases below, we
 * can conclude that the following conditions are required for a
 * significant performance impact of the src array thread locality
 * constraint:
 * - Size of array must be of compile-time known constant size, otherwise
 *   and at most of size EliminateAllocationArraySizeLimit, otherwise,
 *   the array cannot be eliminated.
 * - Size of the arraycopy needs to be larger than ArrayCopyLoadStoreMaxElem,
 *   otherwise it gets expanded anyway, and the load can be optimized with
 *   the expanded load/store pairs.
 * - src array needs to escape, dst array should not escape,
 *   If both escape, the arraycopy needs to be performed anyway.
 *   If src does not escape, we are allowed to optimize dst loads to src loads.
 *
 * Working with "large" arrays: length=200
 * These are too large for elimination, and the performance impact is
 * negligible.
 *
 *   When both arrays escape, then we cannot eliminate arraycopy anyway,
 *   so the effect of not optimizing the dstI load is negligible:
 *   - test200BothEscape100
 *       [AFFECTED]: dstI load not optimized away any more
 *   - test200BothEscape16
 *       [AFFECTED]: dstI load not optimized away any more
 *   - test200BothEscape4   - arraycopy is expanded to load/store pairs
 *
 *   More impactful is when the dstI does not escape, and we could have
 *   been smarter:
 *   - test200OneEscapes100             - but allocation is too large anyway
 *   - test200OneEscapes16              - but allocation is too large anyway
 *   - test200OneEscapes4               - but allocation is too large anyway
 *
 *   And in this scenario, a trap after the arraycopy can mean we would need
 *   to do rematerialization after elimination. But that only works for
 *   arrays that are small enough.
 *   - test200OneEscapesWithSrcStore100 - but allocation is too large anyway
 *   - test200OneEscapesWithSrcStore16  - but allocation is too large anyway
 *   - test200OneEscapesWithSrcStore4   - but allocation is too large anyway
 *
 *   And in some cases, dynamic length prevents the optimizations,
 *   we could consider dedicated optimizations for them in the future:
 *   - test200OneEscapesCopyOf100
 *   - test200OneEscapesCopyOf16
 *   - test200OneEscapesCopyOf4
 *   - test200OneEscapesClone
 *
 *   If the arrays are fully local, we can do the optimization, but
 *   not if the arrays are too large to do elimination:
 *   - test200NoneEscape100
 *   - test200NoneEscape16
 *   - test200NoneEscape4
 *
 * Working with "medium" arrays: length=32
 * These are small enough for allocation elimination, and so the impact
 * of loads from arraycopy that are not optimized could have a much
 * larger performance impact.
 *
 *   When both arrays escape, then we cannot eliminate arraycopy anyway,
 *   so the effect of not optimizing the dstI load is negligible:
 *   - test32BothEscape32
 *       [AFFECTED]: dstI load not optimized away any more
 *   - test32BothEscape16
 *       [AFFECTED]: dstI load not optimized away any more
 *   - test32BothEscape4   - arraycopy is expanded to load/store pairs
 *
 *   More impactful is when the dstI does not escape, and we could have
 *   been smarter:
 *   - test32OneEscapes32
 *       [AFFECTED]: allocation and arraycopy not optimized away any more,
 *                   due to dstI load that is not optimized any more.
 *   - test32OneEscapes16
 *       [AFFECTED]: allocation and arraycopy not optimized away any more,
 *                   due to dstI load that is not optimized any more.
 *
 *   And in this scenario, a trap after the arraycopy can mean we would need
 *   to do rematerialization after elimination. But that only works for
 *   arrays that are small enough.
 *   - test32OneEscapesWithSrcStore32
 *       [AFFECTED]: allocation and arraycopy not optimized away any more,
 *                   due to dstI load that is not optimized any more.
 *                   Hence, also no rematerialization loads at the
 *                   RC uncommon trap any more.
 *   - test32OneEscapesWithSrcStore16
 *       [AFFECTED]: allocation and arraycopy not optimized away any more,
 *                   due to dstI load that is not optimized any more.
 *                   Hence, also no rematerialization loads at the
 *                   RC uncommon trap any more.
 *
 *   If the arraycopy is small enough, we expand it into scalar loads/stores,
 *   which can optimize directly, without need to optimize dstI load through
 *   the arraycopy.
 *   - test32OneEscapes4
 *   - test32OneEscapesWithSrcStore4
 *
 *   And in some cases, dynamic length prevents the optimizations,
 *   we could consider dedicated optimizations for them in the future:
 *   - test32OneEscapesCopyOf32
 *   - test32OneEscapesCopyOf16
 *   - test32OneEscapesCopyOf4
 *   - test32OneEscapesClone
 *
 *   If the arrays are fully local, we can do the optimization, and
 *   since the arrays are small enough for elimination, we can fully
 *   optimize away all memory operations:
 *   - test200NoneEscape100
 *   - test200NoneEscape16
 *   - test200NoneEscape4
 *
 * Working with "small" arrays: length=4
 * They are small enough for allocation elimination and expansion
 * of the arraycopy to load/store pairs, so we don't need the
 * optimization that moves arraycopy dst loads to src loads,
 * at least if the copy length is known.
 *
 *   When both arrays escape, we still have to keep all the load/store
 *   pairs from the expanded arraycopy:
 *   - test4BothEscape4
 *
 *   If the dst does not escape, we can expand the arraycopy and
 *   fully optimize everything away, except for a single src load,
 *   that replaces the dst load.
 *   - test4OneEscapes4
 *
 *   And in some cases, dynamic length prevents the optimizations,
 *   we could consider dedicated optimizations for them in the future:
 *   - test4OneEscapesCopyOf4
 *   - test4OneEscapesClone
 *
 *   If dst escapes, but we have some RC uncommon trap after the
 *   arraycopy, we get rematerialization loads:
 *   - test4OneEscapesWithSrcStore4
 *
 *   If the arrays are fully local, we can do the optimization, and
 *   since the arrays are small enough for elimination, we can fully
 *   optimize away all memory optimizations:
 *   - test4NoneEscape4
 *
 * I also added some "nested arraycopy" examples, which show that
 * we can get cases where the first arraycopy cannot be eliminated,
 * but all subsequent ones are proven to be copying from the local
 * copy, and so the subsequent allocations/arraycopy can be optimized
 * away:
 * - testNested
 * - testOopNested
 *
 * Related test, that checks rematerialization of eliminated arrays,
 * with dst load in cold path:
 *   TestArrayCopyEliminationUncRematerialization.java
 */
public class TestOptimizeLoadToBeforeArrayCopy {

    // ------------------------- setup for collection of tests --------------------
    @FunctionalInterface
    interface TestFunction {
        Object run();
    }

    Map<String,TestFunction> tests = new HashMap<String,TestFunction>();
    Map<String,Object> golds = new HashMap<String,Object>();

    // ------------------------- setup for input data -----------------------------
    private static final Generator INT_GEN = G.ints();

    public static int[] DST_200_I = new int[200];
    public static int[] SRC_200_I = new int[200];
    public static int[] DST_32_I = new int[32];
    public static int[] SRC_32_I = new int[32];
    public static int[] DST_4_I = new int[4];
    public static int[] SRC_4_I = new int[4];
    public static int V1 = (int)INT_GEN.next();
    public static int V2 = (int)INT_GEN.next();
    public static int V3 = (int)INT_GEN.next();

    public static MyObj[] SRC_200_O = new MyObj[200];
    public static MyObj[] SRC_32_O = new MyObj[32];

    static class MyObj {
        public int i;

        MyObj(int i) {
            this.i = i;
        }
    }

    static {
        G.fill(INT_GEN, DST_200_I);
        G.fill(INT_GEN, SRC_200_I);
        G.fill(INT_GEN, DST_32_I);
        G.fill(INT_GEN, SRC_32_I);
        G.fill(INT_GEN, DST_4_I);
        G.fill(INT_GEN, SRC_4_I);
        Arrays.setAll(SRC_200_O, i -> new MyObj(SRC_200_I[i]));
        Arrays.setAll(SRC_32_O,  i -> new MyObj(SRC_32_I[i]));
    }

    public static void main(String[] args) {
        TestFramework f = new TestFramework();
        f.addCrossProductScenarios(
            Set.of("",
                   "-XX:ArrayCopyLoadStoreMaxElem=0",
                   "-XX:ArrayCopyLoadStoreMaxElem=8",
                   "-XX:ArrayCopyLoadStoreMaxElem=100",
                   "-XX:ArrayCopyLoadStoreMaxElem=200"),
            Set.of("",
                   "-XX:EliminateAllocationArraySizeLimit=0",
                   "-XX:EliminateAllocationArraySizeLimit=8",
                   "-XX:EliminateAllocationArraySizeLimit=100",
                   "-XX:EliminateAllocationArraySizeLimit=200")
        );
        f.start();
    }

    public TestOptimizeLoadToBeforeArrayCopy() {
        tests.put("test200BothEscape100", () -> {
            int[] dstI = DST_200_I.clone();
            int[] srcI = SRC_200_I.clone();
            int res = test200BothEscape100(dstI, srcI, V1);
            return new Object[] {res, dstI, srcI};
        });
        tests.put("test200BothEscape16", () -> {
            int[] dstI = DST_200_I.clone();
            int[] srcI = SRC_200_I.clone();
            int res = test200BothEscape16(dstI, srcI, V1);
            return new Object[] {res, dstI, srcI};
        });
        tests.put("test200BothEscape4", () -> {
            int[] dstI = DST_200_I.clone();
            int[] srcI = SRC_200_I.clone();
            int res = test200BothEscape4(dstI, srcI, V1);
            return new Object[] {res, dstI, srcI};
        });
        tests.put("test200OneEscapes100", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapes100(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test200OneEscapes16", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapes16(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test200OneEscapes4", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapes4(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test200OneEscapesCopyOf100", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapesCopyOf100(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test200OneEscapesCopyOf16", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapesCopyOf16(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test200OneEscapesCopyOf4", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapesCopyOf4(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test200OneEscapesClone", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapesClone(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test200OneEscapesWithSrcStore100", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapesWithSrcStore100(dstI, V1);
            return new Object[] {res, dstI};
        });
        tests.put("test200OneEscapesWithSrcStore16", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapesWithSrcStore16(dstI, V1);
            return new Object[] {res, dstI};
        });
        tests.put("test200OneEscapesWithSrcStore4", () -> {
            int[] dstI = DST_200_I.clone();
            int res = test200OneEscapesWithSrcStore4(dstI, V1);
            return new Object[] {res, dstI};
        });
        tests.put("test200NoneEscape100", () -> {
            int res = test200NoneEscape100(V1, V2, V3);
            return new Object[] {res};
        });
        tests.put("test200NoneEscape16", () -> {
            int res = test200NoneEscape16(V1, V2, V3);
            return new Object[] {res};
        });
        tests.put("test200NoneEscape4", () -> {
            int res = test200NoneEscape4(V1, V2, V3);
            return new Object[] {res};
        });

        tests.put("test32BothEscape32", () -> {
            int[] dstI = DST_32_I.clone();
            int[] srcI = SRC_32_I.clone();
            int res = test32BothEscape32(dstI, srcI, V1);
            return new Object[] {res, dstI, srcI};
        });
        tests.put("test32BothEscape16", () -> {
            int[] dstI = DST_32_I.clone();
            int[] srcI = SRC_32_I.clone();
            int res = test32BothEscape16(dstI, srcI, V1);
            return new Object[] {res, dstI, srcI};
        });
        tests.put("test32BothEscape4", () -> {
            int[] dstI = DST_32_I.clone();
            int[] srcI = SRC_32_I.clone();
            int res = test32BothEscape4(dstI, srcI, V1);
            return new Object[] {res, dstI, srcI};
        });
        tests.put("test32OneEscapes32", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapes32(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test32OneEscapes16", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapes16(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test32OneEscapes4", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapes4(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test32OneEscapesCopyOf32", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapesCopyOf32(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test32OneEscapesCopyOf16", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapesCopyOf16(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test32OneEscapesCopyOf4", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapesCopyOf4(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test32OneEscapesClone", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapesClone(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test32OneEscapesWithSrcStore32", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapesWithSrcStore32(dstI, V1);
            return new Object[] {res, dstI};
        });
        tests.put("test32OneEscapesWithSrcStore16", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapesWithSrcStore16(dstI, V1);
            return new Object[] {res, dstI};
        });
        tests.put("test32OneEscapesWithSrcStore4", () -> {
            int[] dstI = DST_32_I.clone();
            int res = test32OneEscapesWithSrcStore4(dstI, V1);
            return new Object[] {res, dstI};
        });
        tests.put("test32NoneEscape32", () -> {
            int res = test32NoneEscape32(V1, V2, V3);
            return new Object[] {res};
        });
        tests.put("test32NoneEscape16", () -> {
            int res = test32NoneEscape16(V1, V2, V3);
            return new Object[] {res};
        });
        tests.put("test32NoneEscape4", () -> {
            int res = test32NoneEscape4(V1, V2, V3);
            return new Object[] {res};
        });

        tests.put("test4BothEscape4", () -> {
            int[] dstI = DST_4_I.clone();
            int[] srcI = SRC_4_I.clone();
            int res = test4BothEscape4(dstI, srcI, V1);
            return new Object[] {res, dstI, srcI};
        });
        tests.put("test4OneEscapes4", () -> {
            int[] dstI = DST_4_I.clone();
            int res = test4OneEscapes4(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test4OneEscapesCopyOf4", () -> {
            int[] dstI = DST_4_I.clone();
            int res = test4OneEscapesCopyOf4(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test4OneEscapesClone", () -> {
            int[] dstI = DST_4_I.clone();
            int res = test4OneEscapesClone(dstI);
            return new Object[] {res, dstI};
        });
        tests.put("test4OneEscapesWithSrcStore4", () -> {
            int[] dstI = DST_4_I.clone();
            int res = test4OneEscapesWithSrcStore4(dstI, V1);
            return new Object[] {res, dstI};
        });
        tests.put("test4NoneEscape4", () -> {
            int res = test4NoneEscape4(V1, V2, V3);
            return new Object[] {res};
        });

        tests.put("testNested", () -> {
            int[] srcI = SRC_200_I.clone();
            int res = testNested(srcI);
            return new Object[] {res, srcI};
        });

        tests.put("testOop32OneEscapes32", () -> {
            MyObj[] srcO = SRC_32_O.clone();
            MyObj res = testOop32OneEscapes32(srcO);
            return new Object[] {res, srcO};
        });
        tests.put("testOopNested", () -> {
            MyObj[] srcO = SRC_200_O.clone();
            int res = testOopNested(srcO);
            return new Object[] {res, srcO};
        });


        // Compute gold value for all test methods before compilation
        for (Map.Entry<String,TestFunction> entry : tests.entrySet()) {
            String name = entry.getKey();
            TestFunction test = entry.getValue();
            Object gold = test.run();
            golds.put(name, gold);
        }
    }

    @Run(test = {"test200BothEscape100",
                 "test200BothEscape16",
                 "test200BothEscape4",
                 "test200OneEscapes100",
                 "test200OneEscapes16",
                 "test200OneEscapes4",
                 "test200OneEscapesCopyOf100",
                 "test200OneEscapesCopyOf16",
                 "test200OneEscapesCopyOf4",
                 "test200OneEscapesClone",
                 "test200OneEscapesWithSrcStore100",
                 "test200OneEscapesWithSrcStore16",
                 "test200OneEscapesWithSrcStore4",
                 "test200NoneEscape100",
                 "test200NoneEscape16",
                 "test200NoneEscape4",
                 "test32BothEscape32",
                 "test32BothEscape16",
                 "test32BothEscape4",
                 "test32OneEscapes32",
                 "test32OneEscapes16",
                 "test32OneEscapes4",
                 "test32OneEscapesCopyOf32",
                 "test32OneEscapesCopyOf16",
                 "test32OneEscapesCopyOf4",
                 "test32OneEscapesClone",
                 "test32OneEscapesWithSrcStore32",
                 "test32OneEscapesWithSrcStore16",
                 "test32OneEscapesWithSrcStore4",
                 "test32NoneEscape32",
                 "test32NoneEscape16",
                 "test32NoneEscape4",
                 "test4BothEscape4",
                 "test4OneEscapes4",
                 "test4OneEscapesCopyOf4",
                 "test4OneEscapesClone",
                 "test4OneEscapesWithSrcStore4",
                 "test4NoneEscape4",
                 "testNested",
                 "testOop32OneEscapes32",
                 "testOopNested"})
    public void run() {
        for (Map.Entry<String,TestFunction> entry : tests.entrySet()) {
            String name = entry.getKey();
            TestFunction test = entry.getValue();
            // Recall gold value from before compilation
            Object gold = golds.get(name);
            // Compute new result
            Object result = test.run();
            // Compare gold and new result
            try {
                Verify.checkEQ(gold, result);
            } catch (VerifyException e) {
                throw new RuntimeException("Verify failed for " + name, e);
            }
        }
    }

    // -------------------------------- length=100 -------------------------------------
    @Test
    // DEFAULT flag range.
    // By default, we cannot get rid of the arraycopy and also have to keep the load.
    //
    // Note: it might be tempting to allow dstI[2] to jump over the arraycopy and
    // load from src directly. But that's not multi-threading safe. It would
    // create a fresh load from src, and could lead to an inconsistent result
    // between the final element value in dstI[2] and the return value - but
    // they must be consistent.
    @IR(counts = {IRNode.LOAD, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  100"})
    // If we allow the arraycopy to expand to load/store pairs, then we get 100 loads from the arraycopy
    // of which srcI[2] folds with the store "srcI[2] = v", so we have 99 loads from
    // arraycopy left. Plus the dstI[2] load, which in theory could have been optimized
    // away, but the traversal up the memory graph is capped, MemNode::find_previous_store
    // has a 50-hop limit, and we would have to traverse through nearly 100 stores.
    @IR(counts = {IRNode.LOAD, "=100", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 100"})
    static int test200BothEscape100(int[] dstI, int[] srcI, int v) {
        srcI[2] = v;
        System.arraycopy(srcI, 0, dstI, 0, 100);
        return dstI[2];
    }

    @Test
    // DEFAULT flag range.
    // By default, we cannot get rid of the arraycopy and also have to keep the load.
    //
    // Note: it might be tempting to allow dstI[2] to jump over the arraycopy and
    // load from src directly. But that's not multi-threading safe. It would
    // create a fresh load from src, and could lead to an inconsistent result
    // between the final element value in dstI[2] and the return value - but
    // they must be consistent.
    @IR(counts = {IRNode.LOAD, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  16"})
    // If we allow the arraycopy to expand to load/store pairs, then we get 16 loads from the arraycopy
    // plus one load for dstI[2], which can common.
    // Interesting: the loads for dstI[2] and srcI[2] are optimized away, because they
    // can directly consume the srcI[2] store value v.
    @IR(counts = {IRNode.LOAD, "=  15", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 16"})
    static int test200BothEscape16(int[] dstI, int[] srcI, int v) {
        srcI[2] = v;
        System.arraycopy(srcI, 0, dstI, 0, 16);
        return dstI[2];
    }

    @Test
    // By default, we cannot get rid of the arraycopy and also have to keep the load.
    //
    // Note: it might be tempting to allow dstI[2] to jump over the arraycopy and
    // load from src directly. But that's not multi-threading safe. It would
    // create a fresh load from src, and could lead to an inconsistent result
    // between the final element value in dstI[2] and the return value - but
    // they must be consistent.
    @IR(counts = {IRNode.LOAD, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  4"})
    // DEFAULT flag range.
    // If we allow the arraycopy to expand to load/store pairs, then we get 4 loads from the arraycopy
    // plus one load for dstI[2], which can common.
    // Interesting: the loads for dstI[2] and srcI[2] are optimized away, because they
    // can directly consume the srcI[2] store value v.
    @IR(counts = {IRNode.LOAD, "=  3", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 4"})
    static int test200BothEscape4(int[] dstI, int[] srcI, int v) {
        srcI[2] = v;
        System.arraycopy(srcI, 0, dstI, 0, 4);
        return dstI[2];
    }

    @Test
    // DEFAULT flag range.
    // In this case, we could in theory remove the copy/allocation: if we were able to
    // realize that only a single load per element is required.
    //
    // But for now: The dstI[2] load is not optimized to a srcI[2] load, and so dstI still has
    // a load use, and the allocation/arraycopy cannot be eliminated.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  100"})
    // Arraycopy expanded to load/store pairs: end up with 100 loads from arraycopy plus one from dstI[2] load.
    // The dstI[2] could in theory be optimized, but the 50-hop limit in MemNode::find_previous_store
    // prevents the traversal through nearly 100 stores.
    // And hence, the remaining dstI load prevents the allocation elimination.
    @IR(counts = {IRNode.LOAD_I, "=101", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 100"})
    static int test200OneEscapes100(int[] srcI) {
        int[] dstI = new int[200];
        System.arraycopy(srcI, 0, dstI, 0, 100);
        return dstI[2];
    }

    @Test
    // In this case, we could in theory remove the copy/allocation: if we were able to
    // realize that only a single load per element is required.
    //
    // DEFAULT flag range.
    // Load after arraycopy prevents array allocation elimination.
    // And limit for arraycopy expansion is too low as well.
    // So for now: The dstI[2] load is not optimized to a srcI[2] load, and so dstI still has
    // a load use, and the allocation/arraycopy cannot be eliminated.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  16"})
    // Threshold for array elimination too high.
    // Arraycopy expanded to load/store pairs: end up with 16 loads from arraycopy plus one from dstI[2] load.
    // The dstI[2] load can be optimized away, because of the expanded arraycopy store to
    // dstI[2]: we can just take its input value instead.
    @IR(counts = {IRNode.LOAD_I, "= 16", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 200", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    // If we set the size limit for arraycopy really high, we can get rid of all arraycopy and allocations.
    // Only the dstI[2], optimized to srcI[2], remains for the return value.
    // Compared to test200OneEscapes100, the dstI load can be optimized, and that leads to allocation elimination.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 200", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    static int test200OneEscapes16(int[] srcI) {
        int[] dstI = new int[200];
        System.arraycopy(srcI, 0, dstI, 0, 16);
        return dstI[2];
    }

    @Test
    // In this case, we could in theory remove the copy/allocation: if we were able to
    // realize that only a single load per element is required.
    //
    // Load after arraycopy prevents array allocation elimination.
    // And limit for arraycopy expansion is too low as well.
    // So for now: The dstI[2] load is not optimized to a srcI[2] load, and so dstI still has
    // a load use, and the allocation/arraycopy cannot be eliminated.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  4"})
    // DEFAULT flag range.
    // Threshold for array elimination too high.
    // Arraycopy expanded to load/store pairs: end up with 4 loads from arraycopy plus one from dstI[2] load.
    // The dstI[2] load can be optimized away, because of the expanded arraycopy store to
    // dstI[2]: we can just take its input value instead.
    @IR(counts = {IRNode.LOAD_I, "=  4", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 200", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    // If we set the size limit for arraycopy really high, we can get rid of all arraycopy and allocations.
    // Only the dstI[2], optimized to srcI[2], remains for the return value.
    // Compared to test200OneEscapes100, the dstI load can be optimized, and that leads to allocation elimination.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 200", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    static int test200OneEscapes4(int[] srcI) {
        int[] dstI = new int[200];
        System.arraycopy(srcI, 0, dstI, 0, 4);
        return dstI[2];
    }

    @Test
    // In theory, we could optimize dstI[2] load to either load from srcI or be zero if srcI too short.
    //
    // The length of the underlying arraycopy is dynamic: Math.min(original.length, newLength)
    // ArrayCopyNode::Ideal requires the get_count (length of copy) to be constant, and <= ArrayCopyLoadStoreMaxElem.
    // One load is a peeled srcI[0] load from arraycopy, the other load is dstI[2].
    // Note: there are a jint and a jlong arraycopy version, on different paths.
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "false"})
    static int test200OneEscapesCopyOf100(int[] srcI) {
        int[] dstI = Arrays.copyOf(srcI, 100);
        return dstI[2];
    }

    @Test
    // In theory, we could optimize dstI[2] load to either load from srcI or be zero if srcI too short.
    //
    // The length of the underlying arraycopy is dynamic: Math.min(original.length, newLength)
    // ArrayCopyNode::Ideal requires the get_count (length of copy) to be constant, and <= ArrayCopyLoadStoreMaxElem.
    // One load is a peeled srcI[0] load from arraycopy, the other load is dstI[2].
    // Note: there are a jint and a jlong arraycopy version, on different paths.
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "false"})
    static int test200OneEscapesCopyOf16(int[] srcI) {
        int[] dstI = Arrays.copyOf(srcI, 16);
        return dstI[2];
    }

    @Test
    // In theory, we could optimize dstI[2] load to either load from srcI or be zero if srcI too short.
    //
    // The length of the underlying arraycopy is dynamic: Math.min(original.length, newLength)
    // ArrayCopyNode::Ideal requires the get_count (length of copy) to be constant, and <= ArrayCopyLoadStoreMaxElem.
    // One load is a peeled srcI[0] load from arraycopy, the other load is dstI[2].
    // Note: there are a jint and a jlong arraycopy version, on different paths.
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "false"})
    static int test200OneEscapesCopyOf4(int[] srcI) {
        int[] dstI = Arrays.copyOf(srcI, 4);
        return dstI[2];
    }

    @Test
    // In theory, we could optimize dstI[2] to load from srcI, or throw if srcI too short.
    //
    // Allocation elimination and arraycopy expansion are blocked by dynamic size of srcI clone.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"})
    static int test200OneEscapesClone(int[] srcI) {
        int[] dstI = srcI.clone();
        return dstI[2];
    }

    @Test
    // dstI, arraycopy and allocation could in theory be eliminated, because dstI[2] is the only load
    // from any element.
    //
    // DEFAULT flag range.
    // Same as test200OneEscapes100, except for "srcI[3] = v".
    // Because srcI not thread local, dstI[2] not optimized, blocks elimination.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  100"})
    // Arraycopy expanded to load/store pairs, but chain too long for dstI[2] load to find its
    // matching store from the expanded arraycopy.
    @IR(counts = {IRNode.LOAD_I, "=101", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 100"})
    static int test200OneEscapesWithSrcStore100(int[] srcI, int v) {
        int[] dstI = new int[200];
        System.arraycopy(srcI, 0, dstI, 0, 100);
        srcI[3] = v; // RC uncommon trap
        return dstI[2];
    }

    @Test
    // dstI, arraycopy and allocation could in theory be eliminated, because dstI[2] is the only load
    // from any element.
    //
    // DEFAULT flag range.
    // Same as test200OneEscapes16, except for "srcI[3] = v".
    // But: the additional srcI[3] store creates an RC uncommon trap, where
    // we have to rematerialize dstI. But dstI is too long for TrackedInitializationLimit,
    // so not all loads can be materialized, which prevents array allocation elimination
    // even if EliminateAllocationArraySizeLimit >= 200.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  16"})
    @IR(counts = {IRNode.LOAD_I, "= 16", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 16"})
    static int test200OneEscapesWithSrcStore16(int[] srcI, int v) {
        int[] dstI = new int[200];
        System.arraycopy(srcI, 0, dstI, 0, 16);
        srcI[3] = v; // RC uncommon trap
        return dstI[2];
    }

    @Test
    // dstI, arraycopy and allocation could in theory be eliminated, because dstI[2] is the only load
    // from any element.
    //
    // Same as test200OneEscapes4, except for "srcI[3] = v".
    // But: the additional srcI[3] store creates an RC uncommon trap, where
    // we have to rematerialize dstI. But dstI is too long for TrackedInitializationLimit,
    // so not all loads can be materialized, which prevents array allocation elimination
    // even if EliminateAllocationArraySizeLimit >= 200.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  4"})
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD_I, "=  4", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 4"})
    static int test200OneEscapesWithSrcStore4(int[] srcI, int v) {
        int[] dstI = new int[200];
        System.arraycopy(srcI, 0, dstI, 0, 4);
        srcI[3] = v; // RC uncommon trap
        return dstI[2];
    }

    @Test
    // DEFAULT flag range.
    // If the array allocation and arraycopy are larger than the thresholds,
    // we cannot expand arraycopy or eliminate allocations.
    // But: we can optimize the dstI[2] load through the arraycopy, because the srcI array is thread local.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 200", "ArrayCopyLoadStoreMaxElem", "<  100"})
    // Arrays not eliminated, but arraycopy expanded to load/store pairs.
    // Elements 0-96 fold to captured stores or zero initialization. Elements 97-99 are beyond
    // TrackedInitializationLimit = 50, i.e. beyond 400 bytes, so we keep those 3 loads.
    // dstI[2] can be optimized away.
    @IR(counts = {IRNode.LOAD_I, "=  3", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 200", "ArrayCopyLoadStoreMaxElem", ">= 100", "UseCompactObjectHeaders", "true"})
    // Without COH, everything is shifted by 4 bytes, and so even fewer loads are captured.
    @IR(counts = {IRNode.LOAD_I, "=  4", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 200", "ArrayCopyLoadStoreMaxElem", ">= 100", "UseCompactObjectHeaders", "false"})
    // If we set the size limit for arraycopy really high, we can get rid of all arraycopy and allocations.
    // Accordingly, all loads can be optimized away.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"EliminateAllocationArraySizeLimit", ">= 200"})
    static int test200NoneEscape100(int v1, int v2, int v3) {
        int[] srcI = new int[200];
        srcI[1] = v1;
        srcI[2] = v2;
        srcI[3] = v3;
        int[] dstI = new int[200];
        System.arraycopy(srcI, 0, dstI, 0, 100);
        return dstI[2];
    }

    @Test
    // DEFAULT flag range.
    // If the array allocation and arraycopy are larger than the thresholds,
    // we cannot expand arraycopy or eliminate allocations.
    // But: we can optimize the dstI[2] load through the arraycopy, because the srcI array is thread local.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 200", "ArrayCopyLoadStoreMaxElem", "<  16"})
    // Arrays not eliminated, but arraycopy expanded to load/store pairs.
    // All loads from arraycopy are optimized to v1-v3 or zero initialization.
    // dstI[2] is optimized to v2.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 200", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    // If we set the size limit for arraycopy really high, we can get rid of all arraycopy and allocations.
    // Accordingly, all loads can be optimized away.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"EliminateAllocationArraySizeLimit", ">= 200"})
    static int test200NoneEscape16(int v1, int v2, int v3) {
        int[] srcI = new int[200];
        srcI[1] = v1;
        srcI[2] = v2;
        srcI[3] = v3;
        int[] dstI = new int[200];
        System.arraycopy(srcI, 0, dstI, 0, 16);
        return dstI[2];
    }

    @Test
    // If the array allocation and arraycopy are larger than the thresholds,
    // we cannot expand arraycopy or eliminate allocations.
    // But: we can optimize the dstI[2] load through the arraycopy, because the srcI array is thread local.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 200", "ArrayCopyLoadStoreMaxElem", "<  4"})
    // DEFAULT flag range.
    // Arrays not eliminated, but arraycopy expanded to load/store pairs.
    // All loads from arraycopy are optimized to v1-v3 or zero initialization.
    // dstI[2] is optimized to v2.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 200", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    // If we set the size limit for arraycopy really high, we can get rid of all arraycopy and allocations.
    // Accordingly, all loads can be optimized away.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"EliminateAllocationArraySizeLimit", ">= 200"})
    static int test200NoneEscape4(int v1, int v2, int v3) {
        int[] srcI = new int[200];
        srcI[1] = v1;
        srcI[2] = v2;
        srcI[3] = v3;
        int[] dstI = new int[200];
        System.arraycopy(srcI, 0, dstI, 0, 4);
        return dstI[2];
    }

    // -------------------------------- length=32 -------------------------------------
    @Test
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  32"})
    @IR(counts = {IRNode.LOAD, "= 31", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 32"})
    static int test32BothEscape32(int[] dstI, int[] srcI, int v) {
        srcI[2] = v;
        System.arraycopy(srcI, 0, dstI, 0, 32);
        return dstI[2];
    }

    @Test
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  16"})
    @IR(counts = {IRNode.LOAD, "=  15", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 16"})
    static int test32BothEscape16(int[] dstI, int[] srcI, int v) {
        srcI[2] = v;
        System.arraycopy(srcI, 0, dstI, 0, 16);
        return dstI[2];
    }

    @Test
    @IR(counts = {IRNode.LOAD, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  4"})
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD, "=  3", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 4"})
    static int test32BothEscape4(int[] dstI, int[] srcI, int v) {
        srcI[2] = v;
        System.arraycopy(srcI, 0, dstI, 0, 4);
        return dstI[2];
    }

    @Test
    // DEFAULT flag range.
    // Thresholds too small for optimization.
    // Note: 1 load for COH alignment of arraycopy (12 byte offset from array object start)
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"ArrayCopyLoadStoreMaxElem", "< 32", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"ArrayCopyLoadStoreMaxElem", "< 32", "UseCompactObjectHeaders", "false"})
    @IR(counts = {IRNode.LOAD_I, "= 32", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "<  32", "ArrayCopyLoadStoreMaxElem", ">= 32"})
    // Require both arraycopy expansion and allocation elimination.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 32", "ArrayCopyLoadStoreMaxElem", ">= 32"})
    static int test32OneEscapes32(int[] srcI) {
        int[] dstI = new int[32];
        System.arraycopy(srcI, 0, dstI, 0, 32);
        return dstI[2];
    }

    @Test
    // DEFAULT flag range.
    // Thresholds too small for optimization.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  16"})
    @IR(counts = {IRNode.LOAD_I, "= 16", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 32", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 32", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    static int test32OneEscapes16(int[] srcI) {
        int[] dstI = new int[32];
        System.arraycopy(srcI, 0, dstI, 0, 16);
        return dstI[2];
    }

    @Test
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  4"})
    @IR(counts = {IRNode.LOAD_I, "=  4", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 32", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    // DEFAULT flag range.
    // We get both arraycopy expansion and allocation elimination.
    // We are left with only one load from srcI[2], but not due to the "look through arraycopy"
    // optimization from LoadNode::can_see_arraycopy_value, rather just because of arraycopy expansion.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 32", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    static int test32OneEscapes4(int[] srcI) {
        int[] dstI = new int[32];
        System.arraycopy(srcI, 0, dstI, 0, 4);
        return dstI[2];
    }

    @Test
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "false"})
    static int test32OneEscapesCopyOf32(int[] srcI) {
        int[] dstI = Arrays.copyOf(srcI, 32);
        return dstI[2];
    }

    @Test
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "false"})
    static int test32OneEscapesCopyOf16(int[] srcI) {
        int[] dstI = Arrays.copyOf(srcI, 16);
        return dstI[2];
    }

    @Test
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "false"})
    static int test32OneEscapesCopyOf4(int[] srcI) {
        int[] dstI = Arrays.copyOf(srcI, 4);
        return dstI[2];
    }

    @Test
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"})
    static int test32OneEscapesClone(int[] srcI) {
        int[] dstI = srcI.clone();
        return dstI[2];
    }

    @Test
    // DEFAULT flag range.
    // Thresholds too small for optimization.
    // Note: 1 load for COH alignment of arraycopy (12 byte offset from array object start)
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"ArrayCopyLoadStoreMaxElem", "<  32", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"ArrayCopyLoadStoreMaxElem", "<  32", "UseCompactObjectHeaders", "false"})
    @IR(counts = {IRNode.LOAD_I, "= 32", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "<  32", "ArrayCopyLoadStoreMaxElem", ">= 32"})
    // Require both arraycopy expansion and allocation elimination.
    // But we still have stores for rematerialization at uncommon trap.
    @IR(counts = {IRNode.LOAD_I, "= 32", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 32", "ArrayCopyLoadStoreMaxElem", ">= 32"})
    static int test32OneEscapesWithSrcStore32(int[] srcI, int v) {
        int[] dstI = new int[32];
        System.arraycopy(srcI, 0, dstI, 0, 32);
        srcI[3] = v; // RC uncommon trap
        return dstI[2];
    }

    @Test
    // DEFAULT flag range.
    // Thresholds too small for optimization.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  16"})
    @IR(counts = {IRNode.LOAD_I, "= 16", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "<  32", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    // Require both arraycopy expansion and allocation elimination.
    // But we still have stores for rematerialization at uncommon trap.
    @IR(counts = {IRNode.LOAD_I, "= 16", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 32", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    static int test32OneEscapesWithSrcStore16(int[] srcI, int v) {
        int[] dstI = new int[32];
        System.arraycopy(srcI, 0, dstI, 0, 16);
        srcI[3] = v; // RC uncommon trap
        return dstI[2];
    }

    @Test
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  4"})
    @IR(counts = {IRNode.LOAD_I, "=  4", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "<  32", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    // DEFAULT flag range.
    // We get both arraycopy expansion and allocation elimination.
    // But we still have stores for rematerialization at uncommon trap.
    @IR(counts = {IRNode.LOAD_I, "=  4", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 32", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    static int test32OneEscapesWithSrcStore4(int[] srcI, int v) {
        int[] dstI = new int[32];
        System.arraycopy(srcI, 0, dstI, 0, 4);
        srcI[3] = v; // RC uncommon trap
        return dstI[2];
    }

    @Test
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 32", "ArrayCopyLoadStoreMaxElem", "<  32"})
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 32", "ArrayCopyLoadStoreMaxElem", ">= 32"})
    // DEFAULT flag range.
    // Arrays fully local, and small enough -> can optimize dstI load, and eliminate allocation/arraycopy.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"EliminateAllocationArraySizeLimit", ">= 32"})
    static int test32NoneEscape32(int v1, int v2, int v3) {
        int[] srcI = new int[32];
        srcI[1] = v1;
        srcI[2] = v2;
        srcI[3] = v3;
        int[] dstI = new int[32];
        System.arraycopy(srcI, 0, dstI, 0, 32);
        return dstI[2];
    }

    @Test
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 32", "ArrayCopyLoadStoreMaxElem", "<  16"})
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 32", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    // DEFAULT flag range.
    // Arrays fully local, and small enough -> can optimize dstI load, and eliminate allocation/arraycopy.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"EliminateAllocationArraySizeLimit", ">= 32"})
    static int test32NoneEscape16(int v1, int v2, int v3) {
        int[] srcI = new int[32];
        srcI[1] = v1;
        srcI[2] = v2;
        srcI[3] = v3;
        int[] dstI = new int[32];
        System.arraycopy(srcI, 0, dstI, 0, 16);
        return dstI[2];
    }

    @Test
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 32", "ArrayCopyLoadStoreMaxElem", "<  4"})
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 32", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    // DEFAULT flag range.
    // Arrays fully local, and small enough -> can optimize dstI load, and eliminate allocation/arraycopy.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"EliminateAllocationArraySizeLimit", ">= 32"})
    static int test32NoneEscape4(int v1, int v2, int v3) {
        int[] srcI = new int[32];
        srcI[1] = v1;
        srcI[2] = v2;
        srcI[3] = v3;
        int[] dstI = new int[32];
        System.arraycopy(srcI, 0, dstI, 0, 4);
        return dstI[2];
    }

    // -------------------------------- length=4 -------------------------------------
    @Test
    @IR(counts = {IRNode.LOAD, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  4"})
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD, "=  3", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", ">= 4"})
    static int test4BothEscape4(int[] dstI, int[] srcI, int v) {
        srcI[2] = v;
        System.arraycopy(srcI, 0, dstI, 0, 4);
        return dstI[2];
    }

    @Test
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"ArrayCopyLoadStoreMaxElem", "<  4", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"ArrayCopyLoadStoreMaxElem", "<  4", "UseCompactObjectHeaders", "false"})
    @IR(counts = {IRNode.LOAD_I, "=  4", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 4", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 4", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    static int test4OneEscapes4(int[] srcI) {
        int[] dstI = new int[4];
        System.arraycopy(srcI, 0, dstI, 0, 4);
        return dstI[2];
    }

    @Test
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", ">= 2", IRNode.ALLOC_ARRAY, ">= 1"},
        applyIfAnd = {"TieredCompilation", "true", "UseCompactObjectHeaders", "false"})
    static int test4OneEscapesCopyOf4(int[] srcI) {
        int[] dstI = Arrays.copyOf(srcI, 4);
        return dstI[2];
    }

    @Test
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"})
    static int test4OneEscapesClone(int[] srcI) {
        int[] dstI = srcI.clone();
        return dstI[2];
    }

    @Test
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    @IR(counts = {IRNode.LOAD_I, "=  2", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"ArrayCopyLoadStoreMaxElem", "<  4", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  1", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"ArrayCopyLoadStoreMaxElem", "<  4", "UseCompactObjectHeaders", "false"})
    @IR(counts = {IRNode.LOAD_I, "=  4", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "<  4", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD_I, "=  4", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 4", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    static int test4OneEscapesWithSrcStore4(int[] srcI, int v) {
        int[] dstI = new int[4];
        System.arraycopy(srcI, 0, dstI, 0, 4);
        srcI[3] = v; // RC uncommon trap
        return dstI[2];
    }

    @Test
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 4", "ArrayCopyLoadStoreMaxElem", "<  4"})
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 2"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 4", "ArrayCopyLoadStoreMaxElem", ">= 4"})
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD_I, "=  0", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIf = {"EliminateAllocationArraySizeLimit", ">= 4"})
    static int test4NoneEscape4(int v1, int v2, int v3) {
        int[] srcI = new int[4];
        srcI[1] = v1;
        srcI[2] = v2;
        srcI[3] = v3;
        int[] dstI = new int[4];
        System.arraycopy(srcI, 0, dstI, 0, 4);
        return dstI[2];
    }

    // -------------------------------- nested arraycopy -------------------------------------

    @Test
    // General comment:
    // COH means 0th element starts at offset 12, there is an extra LoadI for arraycopy alignment.
    //
    // No arraycopy or allocation optimization.
    @IR(counts = {IRNode.LOAD_I, "=  21", IRNode.CALL_OF, "arraycopy", "= 5", IRNode.ALLOC_ARRAY, "= 5"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 16", "ArrayCopyLoadStoreMaxElem", "<  16", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  16", IRNode.CALL_OF, "arraycopy", "= 5", IRNode.ALLOC_ARRAY, "= 5"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 16", "ArrayCopyLoadStoreMaxElem", "<  16", "UseCompactObjectHeaders", "false"})
    // No allocation eliminated, but all arraycopy expanded
    @IR(counts = {IRNode.LOAD_I, "= 128", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 5"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 16", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    // srcI->dstI arraycopy is expanded, but allocation not eliminated.
    // We have dst loads remain, so allocation cannot be eliminated.
    @IR(counts = {IRNode.LOAD_I, "=  68", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 16", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD_I, "=  17", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 16", "ArrayCopyLoadStoreMaxElem", "< 16", "UseCompactObjectHeaders", "true"})
    @IR(counts = {IRNode.LOAD_I, "=  16", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 16", "ArrayCopyLoadStoreMaxElem", "< 16", "UseCompactObjectHeaders", "false"})
    static int testNested(int[] srcI) {
        // For default flags:
        // The dstI array is small enough to allow elimination, but the
        // arraycopy is not small enough to be expanded to load/store pairs.
        // So we have to keep the allocation and arraycopy, but all the nested
        // allocations/arraycopy inside testNestedAccessor can be optimized away.
        int[] dstI = new int[64];
        System.arraycopy(srcI, 0, dstI, 0, 64);
        return testNestedAccessor(dstI,  0) +
               testNestedAccessor(dstI, 16) +
               testNestedAccessor(dstI, 32) +
               testNestedAccessor(dstI, 48);
    }

    @ForceInline
    static int testNestedAccessor(int[] dstI, int i) {
        // The nested allocation and arraycopy is also too large for expansion
        // to load/store pairs, but small enough for elimination.
        int[] tmpI = new int[16];
        System.arraycopy(dstI, i, tmpI, 0, 16);
        return tmpI[0] + tmpI[4] + tmpI[8] + tmpI[12];
    }

    // ----------------- Selection of tests from above, but with oop arrays ---------------------

    @Test
    // DEFAULT flag range.
    // Thresholds too small for optimization.
    @IR(counts = {IRNode.LOAD_I, "= 0", IRNode.LOAD_OF_CLASS, "MyObj", "= 0", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIf = {"ArrayCopyLoadStoreMaxElem", "<  32"})
    @IR(counts = {IRNode.LOAD_I, "= 0", IRNode.LOAD_OF_CLASS, "MyObj", "=32", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "<  32", "ArrayCopyLoadStoreMaxElem", ">= 32"})
    // Require both arraycopy expansion and allocation elimination.
    @IR(counts = {IRNode.LOAD_I, "= 0", IRNode.LOAD_OF_CLASS, "MyObj", "= 1", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 0"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 32", "ArrayCopyLoadStoreMaxElem", ">= 32"})
    static MyObj testOop32OneEscapes32(MyObj[] srcO) {
        MyObj[] dstO = new MyObj[32];
        System.arraycopy(srcO, 0, dstO, 0, 32);
        return dstO[2];
    }

    @Test
    // No arraycopy or allocation optimization.
    @IR(counts = {IRNode.LOAD_I, "= 16", IRNode.CALL_OF, "arraycopy", "= 5", IRNode.ALLOC_ARRAY, "= 5"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 16", "ArrayCopyLoadStoreMaxElem", "<  16"})
    // No allocation eliminated, but all arraycopy expanded
    @IR(counts = {IRNode.LOAD_I, "= 16", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 5"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", "< 16", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    // srcO->dstO arraycopy is expanded, but allocation not eliminated.
    // We have dst loads remain, so allocation cannot be eliminated.
    @IR(counts = {IRNode.LOAD_I, "= 16", IRNode.CALL_OF, "arraycopy", "= 0", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 16", "ArrayCopyLoadStoreMaxElem", ">= 16"})
    // DEFAULT flag range.
    @IR(counts = {IRNode.LOAD_I, "= 16", IRNode.CALL_OF, "arraycopy", "= 1", IRNode.ALLOC_ARRAY, "= 1"},
        applyIfAnd = {"EliminateAllocationArraySizeLimit", ">= 16", "ArrayCopyLoadStoreMaxElem", "< 16"})
    static int testOopNested(MyObj[] srcO) {
        // For default flags:
        // The dstO array is small enough to allow elimination, but the
        // arraycopy is not small enough to be expanded to load/store pairs.
        // So we have to keep the allocation and arraycopy, but all the nested
        // allocations/arraycopy inside testNestedAccessor can be optimized away.
        MyObj[] dstO = new MyObj[64];
        System.arraycopy(srcO, 0, dstO, 0, 64);
        return testOopNestedAccessor(dstO,  0) +
               testOopNestedAccessor(dstO, 16) +
               testOopNestedAccessor(dstO, 32) +
               testOopNestedAccessor(dstO, 48);
    }

    @ForceInline
    static int testOopNestedAccessor(MyObj[] dstO, int i) {
        // The nested allocation and arraycopy is also too large for expansion
        // to load/store pairs, but small enough for elimination.
        MyObj[] tmpO = new MyObj[16];
        System.arraycopy(dstO, i, tmpO, 0, 16);
        return tmpO[0].i + tmpO[4].i + tmpO[8].i + tmpO[12].i;
    }

}
