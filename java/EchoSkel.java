import java.util.*;

public class EchoSkel extends Skeleton {

    private static final String[][][] types = new String[][][] {
        { { "String" }, { "String" } },
        { { "List<String>" }, { "String"}}
    };

    public String[] getArgumentTypes (int methodIndex) {
        return types[methodIndex][0];
    }

    public String[] getReturnTypes (int methodIndex) {
        return types[methodIndex][1];
    }

    public int index;

    private Echo impl;

    public EchoSkel (Echo o) {
        impl = o;
    }

    public void handleRequest (CallRequest r) {
        switch (r.methodIndex) {
        case 0:
            System.out.println("Running echo()");
            try {
                r.returnValue = impl.echo((String)r.args[0]);
                r.returnType = 0;
            } catch (Throwable t) {
                r.returnValue = t;
                r.returnType = -1;
            }
            break;
        case 1:
            System.out.println("Running concat()");
            try {
                r.returnValue = impl.concat((List)r.args[0]);
                r.returnType = 0;
            } catch (Throwable t) {
                r.returnValue = t;
                r.returnType = -1;
            }
            break;
        default:
        }
    }
}
