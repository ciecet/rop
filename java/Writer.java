import java.util.*;
import java.io.*;

abstract class Writer implements Cont, Reusable {

    protected Object object;
    protected Cont cont;
    protected int step;

    public Cont start (Object obj, Cont cc) {
        object = obj;
        cont = cc;
        step = 0;
        return this;
    }

    public Cont run (Object ret, Object ctx) {
        return write(ret, (IOContext)ctx);
    }

    public Cont write (Object ret, IOContext ctx) {
        return cont;
    }

    public abstract void release ();
}
