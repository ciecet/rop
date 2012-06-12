import java.util.*;
import java.io.*;

public class New {

    private static final int ICACHE_MAX = 32;
    private static Map reusables = new IdentityHashMap();
    private static Map readers = new IdentityHashMap();
    private static Map writers = new IdentityHashMap();
    private static Byte[] i8s = new Byte[ICACHE_MAX*2];
    private static Short[] i16s = new Short[ICACHE_MAX*2];
    private static Integer[] i32s = new Integer[ICACHE_MAX*2];

    static {
        for (int i = 0; i < ICACHE_MAX*2; i++) {
            i8s[i] = new Byte((byte)(i - ICACHE_MAX));
            i16s[i] = new Short((short)(i - ICACHE_MAX));
            i32s[i] = new Integer(i - ICACHE_MAX);
        }
    }

    private static Object get (Object key, Map pool) {
        List l = (List)pool.get(key);
        if (l != null && l.size() > 0) {
            return l.remove(l.size() - 1);
        }
        return null;
    }

    private static void release (Object key, Object value, Map pool) {
        List l = (List)pool.get(key);
        if (l != null) {
            l.add(value);
            return;
        }
        l = new ArrayList(4);
        l.add(value);
        pool.put(key, l);
    }

    public static void releaseReader (String name, Reader r) {
        release(name, r, readers);
    }

    public static void releaseWriter (String name, Writer w) {
        release(name, w, writers);
    }

    public static void releaseReusable (Reusable o) {
        release(o.getClass(), o, reusables);
    }

    public static Reader i8Reader () {
        Reader r = (Reader)get("i8", readers);
        if (r == null) {
            r = new Reader() {
                public Cont read (Object _, IOContext ctx) {
                    if (ctx.buffer.size() < 1) {
                        return ctx.blocked(this);
                    }
                    return cont.run(New.i8(ctx.buffer.readI8()), ctx);
                }
                public void release () {
                    New.releaseReader("i8", this);
                }
            };
        }
        return r;
    }

    public static Reader i16Reader () {
        Reader r = (Reader)get("i16", readers);
        if (r == null) {
            r = new Reader() {
                public Cont read (Object _, IOContext ctx) {
                    if (ctx.buffer.size() < 2) {
                        return ctx.blocked(this);
                    }
                    return cont.run(New.i16(ctx.buffer.readI16()), ctx);
                }
                public void release () {
                    New.releaseReader("i16", this);
                }
            };
        }
        return r;
    }

    public static Reader i32Reader () {
        Reader r = (Reader)get("i32", readers);
        if (r == null) {
            r = new Reader() {
                public Cont read (Object _, IOContext ctx) {
                    if (ctx.buffer.size() < 4) {
                        return ctx.blocked(this);
                    }
                    return cont.run(New.i32(ctx.buffer.readI32()), ctx);
                }
                public void release () {
                    New.releaseReader("i32", this);
                }
            };
        }
        return r;
    }

    public static Reader i64 () {
        Reader r = (Reader)get("i64", readers);
        if (r == null) {
            r = new Reader() {
                public Cont read (Object _, IOContext ctx) {
                    if (ctx.buffer.size() < 8) {
                        return ctx.blocked(this);
                    }
                    return cont.run(New.i64(ctx.buffer.readI64()), ctx);
                }
                public void release () {
                    New.releaseReader("i64", this);
                }
            };
        }
        return r;
    }

    public static Reader stringReader () {
        Reader r = (Reader)get("String", readers);
        if (r == null) {
            r = new Reader() {
                byte[] str;
                public Cont read (Object _, IOContext ctx) {
                    switch (step) {
                    case 0: // read length
                        if (ctx.buffer.size() < 4) return ctx.blocked(this);
                        str = new byte[ctx.buffer.readI32()];
                        step++;
                    case 1: // read string
                        if (ctx.buffer.size() < str.length) return ctx.blocked(this);
                        ctx.buffer.readBytes(str, 0, str.length);
                        try {
                            return cont.run(new String(str, "UTF8"), ctx);
                        } catch (UnsupportedEncodingException e) {
                            throw new UnsupportedOperationException(
                                    "cannot handle utf8", e);
                        }
                    default: return null;
                    }
                }
                public void release () {
                    str = null;
                    New.releaseReader("String", this);
                }
            };
        }
        return r;
    }

    private static class ListReader extends Reader {
        Reader itemReader;
        List list;
        int size;
        public Cont read (Object ret, IOContext ctx) {
            switch (step) {
            case 0: // read size
                if (ctx.buffer.size() < 4) return ctx.blocked(this);
                size = ctx.buffer.readI32();
                list = new ArrayList(size);;
                step++;
            case 1: // read item
                if (list.size() == size) {
                    return cont.run(list, ctx);
                }
                step++;
                return itemReader.start(this);
            case 2: // add item
                list.add(ret);
                step--;
                return this;
            default: return null;
            }
        }
        public void release () {
            itemReader.release();
            itemReader = null;
            list = null;
            New.releaseReader("List", this);
        }
    }

    public static Reader listReader (Reader ir) {
        ListReader r = (ListReader)get("List", readers);
        if (r == null) {
            r = new ListReader();
        }
        r.itemReader = ir;
        return r;
    }

    private static class MapReader extends Reader {
        Reader keyReader;
        Reader valueReader;
        Map map;
        Object key;
        int size;
        public Cont read (Object ret, IOContext ctx) {
            switch (step) {
            case 0: // read size
                if (ctx.buffer.size() < 4) return ctx.blocked(this);
                size = ctx.buffer.readI32();
                map = new HashMap(size);
                step++;
            case 1: // read key
                if (map.size() == size) return cont.run(map, ctx);
                step++;
                return keyReader.start(this);
            case 2: // read value
                key = ret;
                step++;
                return valueReader.start(this);
            case 3: // 
                map.put(key, ret);
                step = 1;
                return this;
            default: return null;
            }
        }
        public void release () {
            keyReader.release();
            keyReader = null;
            valueReader.release();
            valueReader = null;
            map = null;
            key = null;
            New.releaseReader("Map", this);
        }
    }

    public static Reader mapReader (Reader kr, Reader vr) {
        MapReader mr = (MapReader)get("Map", readers);
        if (mr == null) {
            mr = new MapReader();
        }
        mr.keyReader = kr;
        mr.valueReader = vr;
        return mr;
    }

    public static Reader reader (String name) {
        Reader r = (Reader)get(name, readers);
        if (r == null) {
            try {
                Class cls = Class.forName(name);
                r = (Reader)cls.newInstance();
            } catch (Throwable t) {
                t.printStackTrace();
                throw new UnsupportedOperationException(
                        "Failed to load reader:"+name, t);
            }
        }
        return r;
    }

    public static Writer i8Writer () {
        Writer w = (Writer)get("i8", writers);
        if (w == null) {
            w = new Writer() {
                public Cont write (Object _, IOContext ctx) {
                    if (ctx.buffer.margin() < 1) {
                        return ctx.blocked(this);
                    }
                    ctx.buffer.writeI8(((Byte)object).byteValue());
                    return cont;
                }
                public void release () {
                    object = null;
                    New.releaseWriter("i8", this);
                }
            };
        }
        return w;
    }

    public static Writer i16Writer () {
        Writer w = (Writer)get("i16", writers);
        if (w == null) {
            w = new Writer() {
                public Cont write (Object _, IOContext ctx) {
                    if (ctx.buffer.margin() < 2) {
                        return ctx.blocked(this);
                    }
                    ctx.buffer.writeI16(((Short)object).shortValue());
                    return cont;
                }
                public void release () {
                    object = null;
                    New.releaseWriter("i16", this);
                }
            };
        }
        return w;
    }

    public static Writer i32Writer () {
        Writer w = (Writer)get("i32", writers);
        if (w == null) {
            w = new Writer() {
                public Cont write (Object _, IOContext ctx) {
                    if (ctx.buffer.margin() < 4) {
                        return ctx.blocked(this);
                    }
                    ctx.buffer.writeI32(((Integer)object).intValue());
                    return cont;
                }
                public void release () {
                    New.releaseWriter("i32", this);
                }
            };
        }
        return w;
    }

    public static Writer i64Writer () {
        Writer w = (Writer)get("i64", writers);
        if (w == null) {
            w = new Writer() {
                public Cont write (Object _, IOContext ctx) {
                    if (ctx.buffer.margin() < 8) {
                        return ctx.blocked(this);
                    }
                    ctx.buffer.writeI64(((Long)object).longValue());
                    return cont;
                }
                public void release () {
                    New.releaseWriter("i64", this);
                }
            };
        }
        return w;
    }

    public static Writer stringWriter () {
        Writer w = (Writer)get("String", writers);
        if (w == null) {
            w = new Writer() {
                public Cont start (Object obj, Cont cc) {
                    try {
                        return super.start(((String)obj).getBytes("UTF8"), cc);
                    } catch (UnsupportedEncodingException e) {
                        throw new UnsupportedOperationException(
                                "Cannot encode as utf8", e);
                    }
                }
                public Cont write (Object _, IOContext ctx) {
                    byte[] s = (byte[])object;
                    if (ctx.buffer.margin() < 4 + s.length) {
                        return ctx.blocked(this);
                    }
                    ctx.buffer.writeI32(s.length);
                    ctx.buffer.writeBytes(s, 0, s.length);
                    return cont;
                }
                public void release () {
                    object = null;
                    New.releaseWriter("String", this);
                }
            };
        }
        return w;
    }

    private static class ListWriter extends Writer {
        Writer itemWriter;
        int i;
        public Cont write (Object _, IOContext ctx) {
            List l = (List)object;
            switch (step) {
            case 0: // write length
                if (ctx.buffer.margin() < 4) return ctx.blocked(this);
                ctx.buffer.writeI32(l.size());
                i = 0;
                step++;
            case 1: // write item
                if (i == l.size()) {
                    return cont;
                }
                return itemWriter.start(l.get(i++), this);
            default: return null;
            }
        }
        public void release () {
            itemWriter.release();
            itemWriter = null;
            object = null;
            New.releaseWriter("List", this);
        }
    }

    public static Writer listWriter (Writer iw) {
        ListWriter w = (ListWriter)get("List", writers);
        if (w == null) {
            w = new ListWriter();
        }
        w.itemWriter = iw;
        return w;
    }

    private static class MapWriter extends Writer {
        Writer keyWriter;
        Writer valueWriter;
        private Iterator iter;
        private Object key;
        public Cont write (Object _, IOContext ctx) {
            Map m = (Map)object;
            switch (step) {
            case 0: // write size
                if (ctx.buffer.margin() < 4) return ctx.blocked(this);
                ctx.buffer.writeI32(m.size());
                iter = m.keySet().iterator();
                step++;
            case 1: // write key
                if (iter.hasNext()) {
                    key = iter.next();
                    step++;
                    return keyWriter.start(key, this);
                }
                return cont;
            case 2: // write value
                step--;
                return valueWriter.start(m.get(key), this);
            default: return null;
            }
        }
        public void release () {
            keyWriter.release();
            keyWriter = null;
            valueWriter.release();
            valueWriter = null;
            iter = null;
            key = null;
            object = null;
            New.releaseWriter("Map", this);
        }
    }

    public static Writer mapWriter (Writer kw, Writer vw) {
        MapWriter mw = (MapWriter)get("Map", writers);
        if (mw == null) {
            mw = new MapWriter();
        }
        mw.keyWriter = kw;
        mw.valueWriter = vw;
        return mw;
    }

    public static Writer writer (String name) {
        Writer w = (Writer)get(name, writers);
        if (w == null) {
            try {
                Class cls = Class.forName(name);
                w = (Writer)cls.newInstance();
            } catch (Throwable t) {
                t.printStackTrace();
                throw new UnsupportedOperationException(
                        "Failed to load reader:"+name, t);
            }
        }
        return w;
    }

    public static RequestWriter requestWriter (int h, int oid, int mid) {
        RequestWriter req = (RequestWriter)get(RequestWriter.class, reusables);
        if (req == null) {
            req = new RequestWriter();
        }
        req.messageHead = h;
        req.objectId = oid;
        req.methodIndex = mid;
        return req;
    }

    public static ReturnReader returnReader () {
        ReturnReader ret = (ReturnReader)get(ReturnReader.class, reusables);
        if (ret == null) {
            ret = new ReturnReader();
        }
        return ret;
    }

    public static Request request (int h, Skeleton skel, int mid) {
        Request req = (Request)get(Request.class, reusables);
        if (req == null) {
            req = new Request();
        }
        req.messageHead = (byte)h;
        req.skeleton = skel;
        req.methodIndex = mid;
        req.ret = returnWriter();
        return req;
    }

    public static ReturnWriter returnWriter () {
        ReturnWriter ret = (ReturnWriter)get(ReturnWriter.class, reusables);
        if (ret == null) {
            ret = new ReturnWriter();
        }
        return ret;
    }

    public static MessageReader messageReader (Port p) {
        MessageReader mr = (MessageReader)get(MessageReader.class, reusables);
        if (mr == null) {
            mr = new MessageReader();
        }
        mr.port = p;
        return mr;
    }

    public static Byte i8 (byte i) {
        if (i >= -ICACHE_MAX && i < ICACHE_MAX) {
            return i8s[i + ICACHE_MAX];
        }
        return new Byte(i);
    }

    public static Short i16 (short i) {
        if (i >= -ICACHE_MAX && i < ICACHE_MAX) {
            return i16s[i + ICACHE_MAX];
        }
        return new Short(i);
    }

    public static Integer i32 (int i) {
        if (i >= -ICACHE_MAX && i < ICACHE_MAX) {
            return i32s[i + ICACHE_MAX];
        }
        return new Integer(i);
    }

    public static Long i64 (long i) {
        return new Long(i);
    }
}
