public class StringCodec implements Codec {
    public void write (Channel chn, Object obj) {
        byte[] a = ((String)obj).getBytes();
        chn.writeI32(a.length);
        chn.writeBytes(a, 0, a.length);
    }

    public Object read (Channel chn) {
        int len = chn.readI32();
        byte[] a = new byte[len];
        chn.readBytes(a, 0, a.length);
        return new String(a);
    }
}
