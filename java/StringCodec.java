import java.io.UnsupportedEncodingException;

public class StringCodec implements Codec
{
    public static final StringCodec instance = new StringCodec();

    public Object read (Buffer buf) {
        int len = buf.readI32();
        byte[] str = new byte[len];
        buf.readBytes(str, 0, len);
        try {
            return new String(str, "UTF8");
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
            throw new RemoteException("utf8 not supported");
        }
    }

    public void write (Object obj, Buffer buf) {
        try {
            byte[] str = ((String)obj).getBytes("UTF8");
            buf.writeI32(str.length);
            buf.writeBytes(str, 0, str.length);
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
            throw new RemoteException("utf8 not supported");
        }
    }

}
