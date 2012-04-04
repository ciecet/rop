interface Codec {
    void write (Channel chn, Object obj);
    Object read (Channel chn);
}
