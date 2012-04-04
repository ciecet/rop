public class RemoteInCodec implements Codec {
    public void write (Channel chn, Object obj) {
        chn.writeI32(((RemoteObject)obj).objectId);
    }
    public Object read (Channel chn) {
        return chn.readRemoteIn();
    }
}
