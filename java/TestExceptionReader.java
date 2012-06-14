public class TestExceptionReader extends Reader {

    Reader iReader = New.i32Reader();

    public Cont read (Object ret, IOContext ctx) {
        switch (step) {
        case 0:
            
            step++;
            return iReader.start(this);
        case 1:
            return cont.run(new TestException((Integer)ret), ctx);
        default: return null;
        }
    }

    public void release () {
        New.releaseReader("TestException", this);
    }
}
