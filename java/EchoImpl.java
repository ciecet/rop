import java.util.*;
class EchoImpl implements Exportable, Echo {

    public Skeleton getSkeleton () {
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
