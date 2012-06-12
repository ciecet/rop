public abstract class Skeleton {
    int id;
    int count;
    Object object;

    public abstract Request createRequest (int h, int mid);

    public abstract void call (Request req);
}
