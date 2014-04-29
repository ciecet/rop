package com.alticast.rop;

public class Context {

    private static ThreadLocal threadLocal = new ThreadLocal () {
        protected synchronized Object initialValue () {
            return new Context();
        }
    };
    private static int nextPortId = 1;

    static synchronized int getNextPortId () {
        return nextPortId++;
    }

    static Context get () {
        return (Context)threadLocal.get();
    }

    public static int getPortId () {
        Context ctx = get();
        if (ctx.portId != 0) {
            return ctx.portId;
        } else {
            return ctx.originalPortId;
        }
    }

    /**
     * Returns a registry currently in action.
     * Active registry means that one of skeleton object in the registry
     * is currently processing a request in the calling thread.
     * You may use this registry object as a key representing the current
     * rop session.
     */
    public static Registry getRegistry () {
        return get().registry;
    }

    private Context () {
        originalPortId = getNextPortId();
    }

    /**
     * Original send port id bound to current context.
     */
    final int originalPortId;

    /**
     * ROP instance which this context is running under.
     * i.e. Non-null iff current context is processnig a rop request.
     */
    Registry registry;

    /**
     * Current send port id where any rop request created in this context
     * will be sent from.
     * Valid only when the request is bound to the current registry.
     */
    int portId;
}
