package com.alticast.rop;

import com.alticast.test.*;
import java.util.*;

public class EchoImpl implements Exportable, Echo {

    public static boolean init () {
        Registry.export("Echo", new EchoImpl());
        return true;
    }

    public Skeleton createSkeleton () {
        return new EchoSkel(this);
    }

    public String echo (String msg) {
        return msg;
    }

    public String concat (List words) {
        StringBuffer buf = new StringBuffer();
        for (Iterator iter = words.iterator(); iter.hasNext(); ) {
            buf.append(iter.next().toString());
        }
        return buf.toString();
    }

    public void touchmenot () throws TestException {
        throw new TestException(new Integer(3));
    }

    public void recursiveEcho (String msg, EchoCallback cb) {
        cb.call(msg);
    }

    public void hello (Person p) {
        p.callback.call("Hi~ "+p.name);
    }

    public void asyncEcho (String msg, EchoCallback cb) {
        cb.call("Hi again~ "+msg);
    }

    public int doubleit (int i) {
        return i*2;
    }
}
