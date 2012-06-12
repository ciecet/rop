import java.util.*;

public class RequestWriter extends Writer {
    int messageHead;
    int objectId;
    int methodIndex;
    private List arguments = new ArrayList();
    private int i;

    public void addArg (Object arg, Writer w) {
        arguments.add(arg);
        arguments.add(w);
    }

    public Cont write (Object _, IOContext ctx) {
        switch (step) {
        case 0:
            if (ctx.buffer.margin() < 7) return ctx.blocked(this);
            ctx.buffer.writeI8((byte)messageHead);
            ctx.buffer.writeI32(objectId);
            ctx.buffer.writeI16((short)methodIndex);
            i = 0;
            step++;
        case 1:
            if (i < arguments.size()) {
                Object arg = arguments.get(i*2);
                Writer w = (Writer)arguments.get(i*2+1);
                i += 2;
                return w.start(arg, this);
            }
            return cont;
        default: return null;
        }
    }

    public void release () {
        while (arguments.size() > 0) {
            int size = arguments.size();
            ((Writer)arguments.remove(size-1)).release();
            arguments.remove(size-2);
        }
        New.releaseReusable(this);
    }

}
