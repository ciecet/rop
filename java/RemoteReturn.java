public class RemoteReturn {
    public int id;
    public String[] returnTypes;
    public int returnType;
    public Object returnValue;

    public synchronized void waitForReturn () {
        while (returnValue == null) {
            try {
                wait();
            } catch (InterruptedException e) {
            }
        }
    }
}
