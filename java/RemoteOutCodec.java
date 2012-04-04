public class RemoteOutCodec implements Codec {
    public void write (Channel chn, Object obj) {
        chn.writeRemoteOut((Exportable)obj);
    }
    public Object read (Channel chn) {
        return chn.getRemoteObject(chn.readI32());
    }
}
