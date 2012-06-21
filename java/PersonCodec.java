public class PersonCodec implements Codec {

    public static final PersonCodec instance = new PersonCodec();

    private static final Codec[] codecs = {
        StringCodec.instance,
        new InterfaceCodec(EchoCallbackStub.class)
    };

    public Object read (Buffer buf) {
        Person o = new Person();
        o.name = (String)codecs[0].read(buf);
        o.callback = (EchoCallback)codecs[1].read(buf);
        return o;
    }

    public void write (Object obj, Buffer buf) {
        Person o = (Person)obj;
        codecs[0].write(o.name, buf);
        codecs[1].write(o.callback, buf);
    }
}
