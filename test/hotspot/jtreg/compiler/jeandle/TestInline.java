/*
 * Copyright (c) 2026, the Jeandle-JDK Authors. All Rights Reserved.
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
 */

/**
 * @test test inline feature for jeandle compiler
 * @library /test/lib /
 * @build jdk.test.lib.Asserts jdk.test.whitebox.WhiteBox
 * @run driver jdk.test.lib.helpers.ClassFileInstaller jdk.test.whitebox.WhiteBox
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI
 *                   -XX:CompileCommand=compileonly,compiler.jeandle.TestInline::caller*
 *                   -XX:CompileCommand=compileonly,compiler.jeandle.TestInline::callee*
 *                   -XX:CompileCommand=inline,compiler.jeandle.TestInline::callee*
 *                   -XX:CompileCommand=dontinline,compiler.jeandle.TestInline::blackHole
 *                   -XX:-TieredCompilation -Xbatch
 *                   -XX:+UseJeandleCompiler compiler.jeandle.TestInline
 */

package compiler.jeandle;

import java.lang.reflect.Method;

import jdk.test.lib.Asserts;
import jdk.test.whitebox.WhiteBox;

public class TestInline {
    private static WhiteBox wb = WhiteBox.getWhiteBox();

    public static void main(String[] args) throws Exception {
        testSimpleInline();
        testInlineWithException();
        testInlineWithLock();
        testInlineWithDeopt();
        testInlineWithBranch();
        testInlineWithLoop();
        testInlineChained();
    }

    // -------------------------------------------------------
    // 1. Simple inline: callee returns a computed value
    // -------------------------------------------------------
    public static int calleeSimple(int x) {
        return x * 2 + 1;
    }

    public static int callerSimple() {
        int sum = 0;
        for (int i = 0; i < 100_000; i++) {
            sum += calleeSimple(i);
        }
        return sum;
    }

    static void testSimpleInline() throws Exception {
        int result = callerSimple();
        int expected = 0;
        for (int i = 0; i < 100_000; i++) {
            expected += i * 2 + 1;
        }
        Asserts.assertEquals(result, expected);

        Method m = TestInline.class.getDeclaredMethod("callerSimple");
        Asserts.assertTrue(wb.isMethodCompiled(m));
    }

    // -------------------------------------------------------
    // 2. Inline with exception: callee throws, caller catches
    // -------------------------------------------------------
    public static int calleeThrow(int x) {
        if (x < 0) {
            throw new IllegalArgumentException("negative");
        }
        return x;
    }

    public static int callerWithException() {
        int sum = 0;
        for (int i = -10; i < 100_000; i++) {
            try {
                sum += calleeThrow(i);
            } catch (IllegalArgumentException e) {
                sum += 1;
            }
        }
        return sum;
    }

    static void testInlineWithException() throws Exception {
        int result = callerWithException();
        int expected = 10;
        for (int i = 0; i < 100_000; i++) {
            expected += i;
        }
        Asserts.assertEquals(result, expected);

        Method m = TestInline.class.getDeclaredMethod("callerWithException");
        Asserts.assertTrue(wb.isMethodCompiled(m));
    }

    // -------------------------------------------------------
    // 3. Inline with lock: callee uses synchronized block
    // -------------------------------------------------------
    private static long lockCounter = 0;
    private static final Object lockObj = new Object();

    public static void calleeWithLock(long val) {
        synchronized (lockObj) {
            lockCounter += val;
        }
    }

    public static long callerWithLock() {
        for (int i = 0; i < 100_000; i++) {
            calleeWithLock(i);
        }
        return lockCounter;
    }

    static void testInlineWithLock() throws Exception {
        lockCounter = 0;
        long result = callerWithLock();
        long expected = 0;
        for (int i = 0; i < 100_000; i++) {
            expected += i;
        }
        Asserts.assertEquals(result, expected);

        Method m = TestInline.class.getDeclaredMethod("callerWithLock");
        Asserts.assertTrue(wb.isMethodCompiled(m));
    }

    // -------------------------------------------------------
    // 4. Inline with deoptimization: callee's type assumption
    //    is invalidated at runtime, triggering deopt.
    // -------------------------------------------------------
    interface I { int value(); }

    static class A implements I {
        public int value() { return 1; }
    }

    static class B implements I {
        public int value() { return 2; }
    }

    public static int calleeDeopt(I obj) {
        return obj.value();
    }

    public static int callerDeopt() {
        A a = new A();
        int sum = 0;
        for (int i = 0; i < 100_000; i++) {
            sum += calleeDeopt(a);
        }
        return sum;
    }

    static void testInlineWithDeopt() throws Exception {
        int result = callerDeopt();
        Asserts.assertEquals(result, 100_000);

        Method m = TestInline.class.getDeclaredMethod("callerDeopt");
        Asserts.assertTrue(wb.isMethodCompiled(m));

        // Now call with a different type to trigger deoptimization
        B b = new B();
        int result2 = calleeDeopt(b);
        Asserts.assertEquals(result2, 2);
    }

    // -------------------------------------------------------
    // 5. Inline with branch: callee has conditional logic
    // -------------------------------------------------------
    public static int calleeBranch(int x) {
        if (x % 2 == 0) {
            return x / 2;
        } else {
            return x * 3 + 1;
        }
    }

    public static int callerWithBranch() {
        int sum = 0;
        for (int i = 0; i < 100_000; i++) {
            sum += calleeBranch(i);
        }
        return sum;
    }

    static void testInlineWithBranch() throws Exception {
        int result = callerWithBranch();
        int expected = 0;
        for (int i = 0; i < 100_000; i++) {
            expected += calleeBranch(i);
        }
        Asserts.assertEquals(result, expected);

        Method m = TestInline.class.getDeclaredMethod("callerWithBranch");
        Asserts.assertTrue(wb.isMethodCompiled(m));
    }

    // -------------------------------------------------------
    // 6. Inline with loop: callee contains a loop
    // -------------------------------------------------------
    public static int calleeLoop(int n) {
        int s = 0;
        for (int i = 0; i < n; i++) {
            s += i;
        }
        return s;
    }

    public static int callerWithLoop() {
        int sum = 0;
        for (int i = 0; i < 100_000; i++) {
            sum += calleeLoop(5);
        }
        return sum;
    }

    static void testInlineWithLoop() throws Exception {
        int result = callerWithLoop();
        Asserts.assertEquals(result, 100_000 * 10);

        Method m = TestInline.class.getDeclaredMethod("callerWithLoop");
        Asserts.assertTrue(wb.isMethodCompiled(m));
    }

    // -------------------------------------------------------
    // 7. Chained inline: A calls B calls C, all inlined
    // -------------------------------------------------------
    public static int calleeC(int x) {
        return x + 1;
    }

    public static int calleeB(int x) {
        return calleeC(x) * 2;
    }

    public static int callerChained() {
        int sum = 0;
        for (int i = 0; i < 100_000; i++) {
            sum += calleeB(i);
        }
        return sum;
    }

    static void testInlineChained() throws Exception {
        int result = callerChained();
        int expected = 0;
        for (int i = 0; i < 100_000; i++) {
            expected += (i + 1) * 2;
        }
        Asserts.assertEquals(result, expected);

        Method m = TestInline.class.getDeclaredMethod("callerChained");
        Asserts.assertTrue(wb.isMethodCompiled(m));
    }

    public static void blackHole() {}
}
