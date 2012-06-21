public class VoidCodec implements Codec {

    public static final VoidCodec instance = new VoidCodec();

    public Object read (Buffer buf) {
        return null;
    }
    public void write (Object obj, Buffer buf) {
    }
}
