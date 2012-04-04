public class IntegerCodec implements Codec {
    public void write (Channel chn, Object obj) {
        chn.writeI32(((Integer)obj).intValue());
    }

    public Object read (Channel chn) {
        return new Integer(chn.readI32());
    }
}
