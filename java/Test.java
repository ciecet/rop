import java.io.*;
import java.util.*;

public class Test {
    public static void main (String[] args) {
        test1();
    }

    public static void test1 () {
        try {
            PipedOutputStream pout0 = new PipedOutputStream();
            PipedInputStream pin0 = new PipedInputStream(pout0);
            PipedOutputStream pout1 = new PipedOutputStream();
            PipedInputStream pin1 = new PipedInputStream(pout1);
            System.out.println("Created pipes");
            Channel chn0 = new Channel(pin1, pout0);
            Channel chn1 = new Channel(pin0, pout1);
            System.out.println("Created Channels");

            System.out.println("Register Echo");
            chn1.registerObject("Echo", new EchoImpl());

            System.out.println("Request stub for Echo");
            Echo e = new EchoStub(chn0.getRemoteObject("Echo"));
            System.out.println("Method call");

            System.out.println("got echo?:"+e.echo("hi!").equals("hi!"));
            List a = new ArrayList();
            a.add("a");
            a.add("b");
            a.add("c");
            System.out.println("got concat?:"+e.concat(a).equals("abc"));

            Thread.sleep(5000);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
