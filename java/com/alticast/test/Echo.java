package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;
import com.alticast.test.TestException;
import com.alticast.test.EchoCallback;
import com.alticast.test.Person;
public interface Echo {
    String echo (String msg);
    String concat (List msgs);
    void touchmenot () throws com.alticast.test.TestException;
    void recursiveEcho (String msg, com.alticast.test.EchoCallback cb);
    void hello (com.alticast.test.Person p);
    void asyncEcho (String msg, com.alticast.test.EchoCallback cb);
}
