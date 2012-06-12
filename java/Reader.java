import java.util.*;
import java.io.*;

abstract class Reader implements Cont, Reusable {

    protected Cont cont;
    protected int step;

    public Cont start (Cont cc) {
        cont = cc;
        step = 0;
        return this;
    }

    public Cont run (Object ret, Object ctx) {
        return read(ret, (IOContext)ctx);
    }

    public Cont read (Object ret, IOContext ctx) {
        return cont;
    }

    public abstract void release ();
}
