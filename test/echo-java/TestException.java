package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;

public class TestException extends Exception {
    public Integer i;
    public TestException () {}
    public TestException (Integer arg0) {
        this.i = arg0;
    }
}
