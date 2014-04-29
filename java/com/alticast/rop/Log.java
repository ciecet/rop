package com.alticast.rop;

public class Log implements LogConsts {

    private static final int ED = com.alticast.debug.Log.ED_ROP;

    public static void fatal (Object s) {
        if (F) com.alticast.debug.Log.err(ED, String.valueOf(s));
    }
    public static void error (Object s) {
        if (E) com.alticast.debug.Log.err(ED, String.valueOf(s));
    }
    public static void warn (Object s) {
        if (W) com.alticast.debug.Log.wrn(ED, String.valueOf(s));
    }
    public static void info (Object s) {
        if (I) com.alticast.debug.Log.msg(ED, String.valueOf(s));
    }
    public static void debug (Object s) {
        if (D) com.alticast.debug.Log.dbg(ED, String.valueOf(s));
    }
    public static void trace (Object s) {
        if (T) com.alticast.debug.Log.dbg(ED, String.valueOf(s));
    }
}
