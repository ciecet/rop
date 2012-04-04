import java.util.*;

class EchoStub implements Echo {
    private RemoteObject remoteObject;

    public static final String[] __echo_returntypes = new String[] { "String" };

    public EchoStub (RemoteObject ro) {
        remoteObject = ro;
        Channel chn = ro.channel;
        if (chn.getCodec("List<String>") == null) {
            chn.putCodec("List<String>", new ListCodec(chn.getCodec("String")));
        }
        //if (chn.getSerializer("List<String>") == null) {
        //    chn.setSerializer("List<String>",
        //            new ListSerializer(new StringSerializer()));
        //}
    }

    public String echo (String msg) {
        Channel chn = remoteObject.channel;
        chn.lock();
        chn.writeI8(Channel.CALL);
        RemoteReturn ret = chn.writeRemoteReturn(__echo_returntypes);
        chn.writeI16(remoteObject.objectId);
        chn.writeI8(0);
        chn.getCodec("String").write(chn, msg);
        chn.unlock();

        ret.waitForReturn();
        switch (ret.returnType) {
        case 0:
            return (String)ret.returnValue;
        default:
            return null;
        }
    }

    public String concat (List words) {
        Channel chn = remoteObject.channel;
        chn.lock();
        chn.writeI8(Channel.CALL);
        RemoteReturn ret = chn.writeRemoteReturn(__echo_returntypes);
        chn.writeI16(remoteObject.objectId);
        chn.writeI8(1);
        chn.getCodec("List<String>").write(chn, words);
        chn.unlock();

        ret.waitForReturn();
        switch (ret.returnType) {
        case 0:
            return (String)ret.returnValue;
        default:
            return null;
        }
    }
}

