import java.util.*;

public class ReturnWriter extends Writer {

    List writers = new ArrayList();
    int index = -1;
    Object value = null;

    public void addWriter (Writer w) {
        writers.add(w);
    }

    public Cont write (Object _, IOContext ctx) {
        if (ctx.buffer.margin() < 1) return ctx.blocked(this);
        ctx.buffer.writeI8((byte)((3<<6) | (index & 63)));
        if (index == -1) {
            return cont;
        }
        return ((Writer)writers.get(index)).start(value, cont);
    }

    public void release () {
        for (int i = writers.size() - 1; i >= 0; i--) {
            Writer w = (Writer)writers.get(i);
            w.release();
        }
        writers.clear();
        index = -1;
        value = null;
        New.releaseReusable(this);
    }
}
