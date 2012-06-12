public class Remote {
    int id; // negative
    int count;
    Registry registry;

    protected void finalize () {
        registry.notifyRemoteDestroy(id, count);
    }
};
