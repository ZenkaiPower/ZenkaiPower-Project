package com.zenkaipower.android;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;

/** The desktop BT3TEXT1 named-resource package, including its little-endian index. */
final class GamePak {
    private static final byte[] MAGIC = "BT3TEXT1".getBytes(StandardCharsets.US_ASCII);
    private static final class Entry {
        String name; long offset, size; File loose; byte[] digest;
        Entry(String name,long offset,long size) { this.name=name; this.offset=offset; this.size=size; }
    }
    private static long number(GameIO.Reader in,int bytes) throws IOException {
        long n=0; for(int i=0;i<bytes;i++) n |= (long)in.readUnsignedByte() << (8*i); return n;
    }
    private static void number(OutputStream out,long n,int bytes) throws IOException {
        for(int i=0;i<bytes;i++) out.write((int)(n >>> (8*i)) & 255);
    }
    private static Map<String,Entry> index(GameIO.Reader in) throws IOException {
        byte[] magic=new byte[8]; in.readFully(magic);
        if(!Arrays.equals(magic,MAGIC)) throw new IOException("Invalid resource package");
        long count=number(in,4), total=in.length();
        if(count>100000) throw new IOException("Too many package entries");
        Map<String,Entry> entries=new TreeMap<>();
        for(int i=0;i<count;i++) {
            long n=number(in,4), offset=number(in,8), size=number(in,8);
            if(n<1 || n>=1200 || offset<0 || size<0 || offset>total || size>total-offset)
                throw new IOException("Invalid resource package index");
            byte[] name=new byte[(int)n]; in.readFully(name);
            String path=new String(name,StandardCharsets.UTF_8);
            if(path.startsWith("/") || path.contains("\\") || Arrays.asList(path.split("/")).contains("..")
                    || entries.put(path,new Entry(path,offset,size))!=null)
                throw new IOException("Invalid resource package path");
        }
        long end=in.getFilePointer();
        for(Entry entry:entries.values()) if(entry.offset<end) throw new IOException("Overlapping package index");
        return entries;
    }
    static byte[] read(File pak,String name) throws IOException {
        try(GameIO.Reader in=new GameIO.Reader(pak,"r")) {
            Entry entry=index(in).get(name);
            if(entry==null || entry.size>128*1024*1024) throw new IOException("Missing resource: "+name);
            byte[] bytes=new byte[(int)entry.size]; in.seek(entry.offset); in.readFully(bytes); return bytes;
        }
    }
    private static void publish(File temporary,File target) throws IOException {
        GameIO.publish(temporary,target);
    }
    static void extract(File pak,String name,File target) throws IOException {
        byte[] bytes=read(pak,name);
        GameFiles.mkdir(target.getParentFile());
        File temp=GameIO.temp(".engine-",".part",target.getParentFile());
        try {
            try(FileOutputStream out=GameIO.output(temp)) { out.write(bytes); out.getFD().sync(); }
            publish(temp,target);
        } finally { if(temp.exists()) temp.delete(); }
    }
    private static void collect(File base,File folder,Map<String,Entry> entries) throws IOException {
        if(GameIO.isSymbolicLink(folder)) throw new IOException("Cannot package a linked folder");
        File[] children=folder.listFiles();
        if(children==null) throw new IOException("Cannot read "+folder);
        for(File child:children) {
            if(GameIO.isSymbolicLink(child)) throw new IOException("Cannot package a linked file");
            if(child.isDirectory()) collect(base,child,entries);
            else {
                String name=base.toPath().relativize(child.toPath()).toString().replace(File.separatorChar,'/');
                Entry entry=new Entry(name,0,child.length()); entry.loose=child; entries.put(name,entry);
            }
        }
    }
    private static MessageDigest digest() throws IOException {
        try { return MessageDigest.getInstance("SHA-256"); }
        catch(Exception e) { throw new IOException(e); }
    }
    private static void emptyFolders(File folder) {
        File[] children=folder.listFiles();
        if(children!=null) for(File child:children) if(child.isDirectory() && !GameIO.isSymbolicLink(child)) emptyFolders(child);
        folder.delete(); // Only empty directories can be removed.
    }
    /** Merge loose overrides with an existing package, verify, then remove only packed inputs. */
    static void packLoose(File directory) throws IOException {
        if(!directory.isDirectory()) return;
        File pak=GameIO.file(directory.getPath()+".pak");
        Map<String,Entry> entries=new TreeMap<>();
        if(pak.isFile()) try(GameIO.Reader old=new GameIO.Reader(pak,"r")) { entries.putAll(index(old)); }
        collect(directory,directory,entries);
        if(entries.values().stream().noneMatch(entry -> entry.loose != null)) { emptyFolders(directory); return; }
        long offset=12;
        for(Entry entry:entries.values()) {
            int n=entry.name.getBytes(StandardCharsets.UTF_8).length;
            if(n<1 || n>=1200) throw new IOException("Resource name is too long");
            offset+=20+n;
        }
        File temp=GameIO.temp(".pak-",".part",directory.getParentFile());
        try {
            try(FileOutputStream file=GameIO.output(temp); BufferedOutputStream out=new BufferedOutputStream(file)) {
                out.write(MAGIC); number(out,entries.size(),4);
                for(Entry entry:entries.values()) {
                    byte[] name=entry.name.getBytes(StandardCharsets.UTF_8);
                    number(out,name.length,4); number(out,offset,8); number(out,entry.size,8); out.write(name); offset+=entry.size;
                }
                byte[] buffer=new byte[262144];
                for(Entry entry:entries.values()) {
                    MessageDigest hash=digest();
                    try(InputStream in=entry.loose!=null ? GameIO.input(entry.loose):GameIO.input(pak)) {
                        if(entry.loose==null) ((FileInputStream)in).getChannel().position(entry.offset);
                        long left=entry.size;
                        while(left>0) {
                            int n=in.read(buffer,0,(int)Math.min(left,buffer.length));
                            if(n<0) throw new IOException("Resource changed while packing");
                            out.write(buffer,0,n); hash.update(buffer,0,n); left-=n;
                        }
                    }
                    entry.digest=hash.digest();
                }
                out.flush(); file.getFD().sync();
            }
            try(GameIO.Reader verify=new GameIO.Reader(temp,"r")) {
                Map<String,Entry> packed=index(verify); byte[] buffer=new byte[262144];
                for(Entry entry:entries.values()) {
                    Entry saved=packed.get(entry.name); MessageDigest hash=digest(); verify.seek(saved.offset);
                    long left=saved.size;
                    while(left>0) { int n=verify.read(buffer,0,(int)Math.min(left,buffer.length)); if(n<0) throw new EOFException(); hash.update(buffer,0,n); left-=n; }
                    if(!Arrays.equals(hash.digest(),entry.digest)) throw new IOException("Package verification failed");
                    if(entry.loose!=null) {
                        hash=digest();
                        try(InputStream in=GameIO.input(entry.loose)) { int n; while((n=in.read(buffer))!=-1) hash.update(buffer,0,n); }
                        if(!Arrays.equals(hash.digest(),entry.digest)) throw new IOException("Resource changed while packing");
                    }
                }
            }
            publish(temp,pak);
            for(Entry entry:entries.values()) if(entry.loose!=null && !entry.loose.delete())
                throw new IOException("Packed resource could not be removed: "+entry.name);
            emptyFolders(directory);
        } finally { if(temp.exists()) temp.delete(); }
    }
}
