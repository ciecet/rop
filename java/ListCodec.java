import java.util.*;

public class ListCodec implements Codec {

    private final Codec itemCodec;

    public ListCodec (Codec ic) {
        itemCodec = ic;
    }

    public void write (Channel chn, Object obj) {
        List l = (List)obj;
        chn.writeI32(l.size());
        for (Iterator iter = l.iterator(); iter.hasNext(); ) {
            itemCodec.write(chn, iter.next());
        }
    }

    public Object read (Channel chn) {
        int n = chn.readI32();
        List l = new ArrayList(n);
        for (int i = 0; i < n; i++) {
            l.add(itemCodec.read(chn));
        }
        return l;
    }
}
