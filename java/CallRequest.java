public class CallRequest {
    public Skeleton skeleton;
    public int returnId;
    public int objectId;
    public int methodIndex;
    public Object[] args;

    public int returnType;
    public Object returnValue;

    public int chainIndex;
    public CallRequest previous;
}
