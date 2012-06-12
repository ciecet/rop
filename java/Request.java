import java.util.*;

public class Request extends Reader {
    byte messageHead;
    Skeleton skeleton;
    int methodIndex;
    ReturnWriter ret;
    Request next;

    List arguments = new ArrayList();
    List readers = new ArrayList();

    public void addReader (Reader r) {
        readers.add(r);
    }

    public void call () {
        skeleton.call(this);
    }

    public Cont read (Object arg, IOContext ctx) {
        switch (step) {
        case 0: // read arguments
            if (arguments.size() == readers.size()) return cont;
            step++;
            return ((Reader)readers.get(arguments.size())).start(this);
        case 1: // got value
            arguments.add(arg);
            step = 0;
            return this;
        default: return null;
        }
    }

    public void release () {
        ret.release();
        ret = null;
        arguments.clear();
        for (int i = readers.size() - 1; i >= 0; i--) {
            Reader r = (Reader)readers.get(i);
            r.release();
        }
        readers.clear();
        New.releaseReusable(this);
    }
};
