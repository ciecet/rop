public class TestExceptionWriter extends Writer {

    Writer iWriter = New.i32Writer();

    public Cont write (Object ret, IOContext ctx) {
        return iWriter.start(((TestException)object).i, cont);
    }

    public void release () {
        New.releaseWriter("TestException", this);
    }
}
