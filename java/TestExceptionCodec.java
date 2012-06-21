public class TestExceptionCodec implements Codec {

    public static final TestExceptionCodec instance = new TestExceptionCodec();

    private static final Codec[] codecs = new Codec[] {
        new NullableCodec(I32Codec.instance)
    };

    public Object read (Buffer buf) {
        TestException o = new TestException();
        o.i = (Integer)codecs[0].read(buf);
        return o;
    }

    public void write (Object obj, Buffer buf) {
        TestException o = (TestException)obj;
        codecs[0].write(o.i, buf);
    }
}
