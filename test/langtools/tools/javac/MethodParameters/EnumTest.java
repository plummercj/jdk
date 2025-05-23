/*
 * Copyright (c) 2013, 2024, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @bug 8006582 8008658
 * @summary javac should generate method parameters correctly.
 * @build MethodParametersTester ClassFileVisitor ReflectionVisitor
 * @compile -parameters EnumTest.java
 * @run main MethodParametersTester EnumTest EnumTest.out
 */

/** Test that parameter names are recorded for enum methods */
enum EnumTest {
    E1(0), E2(1, "x"), E3(2, "x", "y"), E4;

    EnumTest() { }
    EnumTest(int a, String... ba) { }
    boolean ok(int c, String... dc) { return true; }

    int valueOf(EnumTest A, EnumTest BA) { return 0; }
}



