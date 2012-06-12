public class IOContext {
    public Buffer buffer;
    public Registry registry;

    private boolean blocked = false;

    public Cont runUntilBlocked (Cont c) {
        while (c != null) {
            if (blocked) {
                blocked = false;
                break;
            }
            c = c.run(null, this);
        }
        return c;
    }

    public Cont blocked (Cont c) {
        blocked = true;
        return c;
    }
}
