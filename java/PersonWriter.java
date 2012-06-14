public class PersonWriter extends Writer {

    Writer nameWriter = New.stringWriter();
    Writer echoCallbackWriter = New.interfaceWriter("EchoCallback");

    public Cont write (Object _, IOContext ctx) {
        Person p = (Person)object;
        switch (step) {
        case 0:
            step++;
            return nameWriter.start(p.name, this);
        case 1:
            step++;
            return echoCallbackWriter.start(p.callback, cont);
        default:
            return null;
        }
    }

    public void release () {
        New.releaseWriter("Person", this);
    }
}
