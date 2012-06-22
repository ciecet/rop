package com.alticast.rop;

public class Log {
    public static void fatal (Object s) {
        System.out.println("!! "+s);
    }
    public static void error (Object s) {
        System.out.println(" ! "+s);
    }
    public static void warning (Object s) {
        System.out.println(" ? "+s);
    }
    public static void info (Object s) {
        System.out.println("    "+s);
    }
    public static void debug (Object s) {
        System.out.println(" . "+s);
    }
    public static void trace (Object s) {
        System.out.println(".. "+s);
    }
}
