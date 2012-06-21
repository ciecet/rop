public class TestException extends Exception {
    public Integer i;

    public TestException () {}
    public TestException (Integer i) {
        this.i = i;
    }
}
