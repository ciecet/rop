public abstract class Skeleton {
    int id;
    int count;
    Object object;

    public abstract void processRequest (LocalCall lc);
}
