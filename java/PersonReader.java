public class PersonReader extends Reader {

    Reader nameReader = New.stringReader();
    Reader echoCallbackReader = New.interfaceReader("EchoCallback");

    Person person;

    public Cont read (Object ret, IOContext ctx) {
        switch (step) {
        case 0:
            person = new Person();
            step++;
            return nameReader.start(this);
        case 1:
            person.name = (String)ret;
            step++;
            return echoCallbackReader.start(this);
        case 2:
            person.callback = (EchoCallback)ret;
            return cont.run(person, ctx);
        default:
            return null;
        }
    }

    public void release () {
        person = null;
        New.releaseReader("Person", this);
    }
}
