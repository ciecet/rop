public class I16Codec implements Codec
{
    public static final I16Codec instance = new I16Codec();

    public Object read (Buffer buf) {
        return New.i16(buf.readI16());
    }

    public void write (Object obj, Buffer buf) {
        buf.writeI16(((Short)obj).shortValue());
    }
}
