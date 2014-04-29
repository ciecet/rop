package com.alticast.rop;

import com.alticast.io.Buffer;

import java.util.*;
import java.io.UnsupportedEncodingException;

public class VariantCodec implements Codec, LogConsts
{
    private static final int TYPE_NULL = 0x80;
    private static final int TYPE_FALSE = 0x82;
    private static final int TYPE_TRUE  = 0x83;
    private static final int TYPE_I8  = 0x84;
    private static final int TYPE_I16 = 0x85;
    private static final int TYPE_I32 = 0x86;
    private static final int TYPE_I64 = 0x87;
    private static final int TYPE_F32 = 0x88;
    private static final int TYPE_F64 = 0x89;

    private static final int MASK_LENGTH_1  = 0x1f;
    private static final int TYPE_INTEGER = 0xe0;
    private static final int TYPE_STRING  = 0xc0;
    private static final int TYPE_LIST    = 0xa0;

    private static final int MASK_LENGTH_2  = 0x0f;
    private static final int TYPE_MAP     = 0x90;

    private static final int TYPE_UNKNOWN = 0x00;

    static class HeaderInfo implements Reusable {
        int type;
        int value;      // value embedded in header.
        int length;     // length of value.

        public void reuse() {
            type = TYPE_UNKNOWN;
            value = 0;
            length = 0;
        }
    }

    // return type, length
    private HeaderInfo parseHeader(Buffer buf) throws RemoteException {
        byte value = buf.readI8();
        int h = value & 0xff;
        int h1 = h & ~MASK_LENGTH_1;
        int h2 = h & ~MASK_LENGTH_2;
        int v1 = h & MASK_LENGTH_1;
        int v2 = h & MASK_LENGTH_2;

        HeaderInfo info = (HeaderInfo)New.get(HeaderInfo.class);

        if (value >= -32) {
            info.type = TYPE_INTEGER;
            info.value  = value;
            info.length = 0;
            return info;
        }

        switch (h1) {
            case TYPE_STRING:
            case TYPE_LIST:
                info.type = h1;
                if (v1 < MASK_LENGTH_1) {
                    info.length = v1;
                } else {
                    info.length = buf.readI32();
                }
                return info;
        }

        switch (h2) {
            case TYPE_MAP:
                info.type = h2;
                if (v2 < MASK_LENGTH_2) {
                    info.length = v2;
                } else {
                    info.length = buf.readI32();
                }
                return info;
        }

        switch (h) {
            case TYPE_NULL:
            case TYPE_FALSE:
            case TYPE_TRUE:
                info.type = h;
                info.length = 0;
                break;
            case TYPE_I8:
                info.type = h;
                info.length = 1;
                break;
            case TYPE_I16:
                info.type = h;
                info.length = 2;
                break;
            case TYPE_I32:
            case TYPE_F32:
                info.type = h;
                info.length = 4;
                break;
            case TYPE_I64:
            case TYPE_F64:
                info.type = h;
                info.length = 8;
                break;
            default:
                if (W) Log.warn("parsed variant: unknown type, parsed to NULL.");
                info.type = TYPE_NULL;
                info.length = 0;
        }

        return info;
    }

    public static final VariantCodec instance = new VariantCodec();

    public void scan (Buffer buf) {
        HeaderInfo info = parseHeader(buf);
        int len = info.length;

        switch(info.type) {
            case TYPE_LIST:
                while(len-- > 0) {
                    scan(buf);
                }
                break;
            case TYPE_MAP:
                while(len-- > 0) {
                    scan(buf);  // scan key
                    scan(buf);  // scan value
                }
                break;
            default:
                buf.drop(info.length);
                break;
        }

        New.release(info);
    }

    public Object read (Buffer buf) {
        HeaderInfo info = parseHeader(buf);
        Object obj = null;

        switch(info.type) {
            case TYPE_INTEGER:
                obj = New.i8((byte)info.value);
                break;
            case TYPE_I8:
                obj = New.i8(buf.readI8());
                break;
            case TYPE_I16:
                obj = New.i16(buf.readI16());
                break;
            case TYPE_I32:
                obj = New.i32(buf.readI32());
                break;
            case TYPE_I64:
                obj = New.i64(buf.readI64());
                break;
            case TYPE_F32:
                obj = New.f32(buf.readF32());
                break;
            case TYPE_F64:
                obj = New.f64(buf.readF64());
                break;
            case TYPE_STRING:
                obj = readString(buf, info.length);
                break;
            case TYPE_TRUE:
                obj = New.bool(true);
                break;
            case TYPE_FALSE:
                obj = New.bool(false);
                break;
            case TYPE_NULL:
                obj = null;
                break;
            case TYPE_LIST:
                obj = readList(buf, info.length);
                break;
            case TYPE_MAP:
                obj = readMap(buf, info.length);
                break;
            default:
                if (W) Log.warn("read variant: unknown type, returning null.");
                obj = null;
        }

        New.release(info);
        return obj;
    }

    private Object readString(Buffer buf, int len) {
        byte[] str = new byte[len];
        buf.readBytes(str, 0, len);

        try {
            return new String(str, "UTF8");
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
            throw new RemoteException("utf8 not supported");
        }
    }

    private Object readList(Buffer buf, int num) {
        List o = new ArrayList(num);
        while (num-- > 0) {
            o.add(read(buf));
        }
        return o;
    }

    private Object readMap(Buffer buf, int num) {
        Map m = new HashMap(num);
        while (num-- > 0) {
            Object k = read(buf);
            Object v = read(buf);
            m.put(k, v);
        }
        return m;
    }

    public void write(Object obj, Buffer buf) {

        if (obj == null) {
            buf.writeI8(TYPE_NULL);
        } else if (obj instanceof Float) {
            writeFloat((Float)obj, buf);
        } else if (obj instanceof Double) {
            writeDouble((Double)obj, buf);
        } else if (obj instanceof Number) {
            writeNumber((Number)obj, buf);
        } else if (obj instanceof String) {
            writeString((String)obj, buf);
        } else if (obj instanceof Boolean) {
            writeBoolean((Boolean)obj, buf);
        } else if (obj instanceof List) {
            writeList((List)obj, buf);
        } else if (obj instanceof Map) {
            writeMap((Map)obj, buf);
        } else {
            if (W) Log.warn("write variant: unknown type, writing null.");
            buf.writeI8(TYPE_NULL);
        }

        return;
    }

    private void writeNumber(Number obj, Buffer buf) {
        long value = obj.longValue();

        if (value >= -32 && value <= Byte.MAX_VALUE) {
            // Integer
            if (T) Log.trace("writing Integer value: "+value);
            buf.writeI8((byte)value);
        } else if (value >= Byte.MIN_VALUE && value < -32) {
            if (T) Log.trace("writing I8 value: "+value);
            buf.writeI8(TYPE_I8);
            buf.writeI8((byte)value);
        } else if (value >= Short.MIN_VALUE && value <= Short.MAX_VALUE) {
            // I16
            if (T) Log.trace("writing I16 value: "+value);
            buf.writeI8(TYPE_I16);
            buf.writeI16((short)value);
        } else if (value >= Integer.MIN_VALUE && value <= Integer.MAX_VALUE) {
            // I32
            if (T) Log.trace("writing I32 value: "+value);
            buf.writeI8(TYPE_I32);
            buf.writeI32((int)value);
        } else {
            // I64
            if (T) Log.trace("writing I64 value: "+value);
            buf.writeI8(TYPE_I64);
            buf.writeI64(value);
        }
    }

    private void writeString(String obj, Buffer buf) {
        try {
            if (T) Log.trace("writing String value: "+obj);

            byte[] str = obj.getBytes("UTF8");
            int strlen = str.length;

            if (strlen < MASK_LENGTH_1) {
                // length is encoded with type.
                buf.writeI8(TYPE_STRING | (byte)str.length);
            } else {
                buf.writeI8(TYPE_STRING | MASK_LENGTH_1);
                buf.writeI32(strlen);
            }
            buf.writeBytes(str, 0, str.length);
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
            throw new RemoteException("utf8 not supported");
        }
    }

    private void writeBoolean(Boolean obj, Buffer buf) {
        if (obj.booleanValue()) {
            // true
            if (T) Log.trace("writing true");
            buf.writeI8(TYPE_TRUE);
        } else {
            // false
            if (T) Log.trace("writing false");
            buf.writeI8(TYPE_FALSE);
        }
    }

    private void writeList(List obj, Buffer buf) {
        if (T) Log.trace("writing List value: "+obj);

        int num = obj.size();
        if (num < MASK_LENGTH_1) {
            buf.writeI8(TYPE_LIST | (byte)num);
        } else {
            buf.writeI8(TYPE_LIST | MASK_LENGTH_1);
            buf.writeI32(num);
        }

        for (int i=0; i<num; i++) {
            write(obj.get(i), buf);
        }
    }

    private void writeMap(Map obj, Buffer buf) {
        if (T) Log.trace("writing Map value: "+obj);

        int num = obj.size();
        if (num < MASK_LENGTH_2) {
            buf.writeI8(TYPE_MAP | (byte)num);
        } else {
            buf.writeI8(TYPE_MAP | MASK_LENGTH_2);
            buf.writeI32(num);
        }

        for (Iterator iter = obj.keySet().iterator(); iter.hasNext();) {
            Object k = iter.next();
            write(k, buf);
            write(obj.get(k), buf);
        }
    }

    private void writeFloat(Float obj, Buffer buf) {
        if (T) Log.trace("writing F32 value: "+obj);
        buf.writeI8(TYPE_F32);
        buf.writeF32(obj.floatValue());
    }

    private void writeDouble(Double obj, Buffer buf) {
        if (T) Log.trace("writing F64 value: "+obj);
        buf.writeI8(TYPE_F64);
        buf.writeF64(obj.doubleValue());
    }
}
