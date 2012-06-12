public abstract class Transport {

    public final Registry registry;

    public Transport () {
        registry = new Registry(this);
    }

    public abstract void send (Port p);
    public abstract void receive (Port p);
    public void notifyUnhandledRequest (Port p) {}
};
