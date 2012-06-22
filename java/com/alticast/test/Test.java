package com.alticast.test;
import com.alticast.rop.*;
import java.io.*;
import java.util.*;

public class Test {

    private static class EchoCallbackImpl implements Exportable, EchoCallback {
        public Skeleton createSkeleton () {
            return new EchoCallbackSkel(this);
        }
        public void call (String msg) {
            System.out.println("GOT CALLBACK:"+msg);
        }
    }

    private static class EchoImpl implements Exportable, Echo {

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

    public static void main (String[] args) {
        test1();
        System.gc();
        System.gc();
        System.gc();
    }

    public static void test1 () {
        try {
            Pipe p0 = new Pipe();
            Pipe p1 = new Pipe();
            Log.info("Created pipes");
            StreamTransport t0 = new StreamTransport(
                    p1.inputStream, p0.outputStream);
            StreamTransport t1 = new StreamTransport(
                    p0.inputStream, p1.outputStream);
            Log.info("Created Transports");

            Log.info("Register Echo");
            t1.registry.registerExportable("Echo", new EchoImpl());

            Log.info("Request stub for Echo");
            EchoStub e = new EchoStub();
            e.remote = t0.registry.getRemote("Echo");

            Log.info("got echo?:"+e.echo("hi!").equals("hi!"));
            List a = new ArrayList();
            a.add("a");
            a.add("b");
            a.add("c");
            Log.info("got concat?:"+e.concat(a).equals("abc"));

            try {
                e.touchmenot();
            } catch (TestException ex) {
                Log.info("got exception:"+ex);
                Log.info("got exception:"+ex.i);
            }

            Person p = new Person();
            p.name = "Sooin";
            p.callback = new EchoCallbackImpl();

            e.recursiveEcho("HELLO WORLD!", p.callback);

            e.hello(p);

            e.asyncEcho("AsyncEchoMessage", p.callback);

            Log.info("15 * 2 : "+e.doubleit(15));

            Thread.sleep(2000);

            Log.info("closing t1");
            t1.dispose();

            Thread.sleep(2000);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
