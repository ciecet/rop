import java.util.*;

public class ReturnReader extends Reader {
    int index = -1;
    Object value = null;
    boolean isValid = false;
    List readers = new ArrayList();
    ReturnReader next;

    public void addReader (Reader r) {
        readers.add(r);
    }

    public Cont start (int h, Cont cc) {
        index = h & 63;
        if (index == 63) {
            index = -1;
            return cc;
        }
        return start(cc);
    }

    public Cont read (Object ret, IOContext ctx) {
        switch (step) {
        case 0:
            step++;
            return ((Reader)readers.get(index)).start(this);
        case 1:
            value = ret;
            isValid = true;
            return cont;
        default: return null;
        }
    }

    public void release () {
        while (readers.size() > 0) {
            ((Reader)readers.remove(readers.size()-1)).release();
        }
        index = -1;
        value = null;
        isValid = false;
        New.releaseReusable(this);
    }
}
