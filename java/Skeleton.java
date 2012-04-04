public abstract class Skeleton {
    int index;
    public abstract String[] getArgumentTypes (int methodIndex);
    public abstract String[] getReturnTypes (int methodIndex);
    public abstract void handleRequest (CallRequest r);
}
