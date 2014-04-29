package com.alticast.rop;

public class Base64 {

    private static byte[] MimeBase64 = new byte[] {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3',
        '4', '5', '6', '7', '8', '9', '+', '/'
    };

    private static int[] DecodeMimeBase64 = new int[] {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
    };

    public static int decode (byte[] text, int off, byte[] dst, int doff,
            int len) {
        int space_idx = 0, phase;
        int prev_d = 0;
        int c;

        phase = 0;

        for (int i = 0; i < len; i++) {
            int d = DecodeMimeBase64[text[off+i] & 0xff];
            if ( d != -1 ) {
                switch ( phase ) {
                    case 0:
                        ++phase;
                        break;
                    case 1:
                        c = ( ( prev_d << 2 ) | ( ( d & 0x30 ) >> 4 ) );
                        if ( space_idx < len )
                            dst[doff + space_idx++] = (byte)c;
                        ++phase;
                        break;
                    case 2:
                        c = ( ( ( prev_d & 0xf ) << 4 ) | ( ( d & 0x3c ) >> 2 ) );
                        if ( space_idx < len )
                            dst[doff + space_idx++] = (byte)c;
                        ++phase;
                        break;
                    case 3:
                        c = ( ( ( prev_d & 0x03 ) << 6 ) | d );
                        if ( space_idx < len )
                            dst[doff + space_idx++] = (byte)c;
                        phase = 0;
                        break;
                }
                prev_d = d;
            }
        }
        return space_idx;
    }

    public static int encode (byte[] src, int soff, byte[] text, int toff,
            int len) {
        byte[] input = new byte[3];
        byte[] output = new byte[4];
        int index, i, j, size;

        size = (4 * (len / 3)) + (((len % 3) > 0)? 4 : 0);
        j = toff;
        for  (i = 0; i < len; i++) {
            index = i % 3;
            input[index] = src[soff+i];
            if (index == 2 || i + 1 == len) {
                output[0] = (byte)((input[0] & 0xFC) >>> 2);
                output[1] = (byte)(((input[0] & 0x3) << 4) | ((input[1] & 0xF0) >>> 4));
                output[2] = (byte)(((input[1] & 0xF) << 2) | ((input[2] & 0xC0) >>> 6));
                output[3] = (byte)(input[2] & 0x3F);

                text[j++] = MimeBase64[output[0]];
                text[j++] = MimeBase64[output[1]];
                text[j++] = index == 0 ? (byte)'=' : MimeBase64[output[2]];
                text[j++] = index <  2 ? (byte)'=' : MimeBase64[output[3]];
                input[0] = input[1] = input[2] = 0;
            }
        }
        return size;
    }
}
