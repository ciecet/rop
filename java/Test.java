import java.io.*;
import java.util.*;

public class Test {
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
            System.out.println("returns:"+buf.toString());
            return buf.toString();
        }
    }

    public static void main (String[] args) {
        test1();
    }

    public static void test1 () {
        try {
            Pipe p0 = new Pipe();
            Pipe p1 = new Pipe();
            System.out.println("Created pipes");
            Transport t0 = new StreamTransport(p1.inputStream, p0.outputStream);
            Transport t1 = new StreamTransport(p0.inputStream, p1.outputStream);
            System.out.println("Created Transports");

            System.out.println("Register Echo");
            t1.registry.registerExportable("Echo", new EchoImpl());

            System.out.println("Request stub for Echo");
            Echo e = new EchoStub(t0.registry.getRemote("Echo"));
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
