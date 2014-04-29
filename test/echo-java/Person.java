package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;
import com.alticast.test.EchoCallback;
public class Person {
    public String name;
    public com.alticast.test.EchoCallback callback;
    public Person () {}
    public Person (String arg0, com.alticast.test.EchoCallback arg1) {
        this.name = arg0;
        this.callback = arg1;
    }
}
