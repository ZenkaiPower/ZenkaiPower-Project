"""Check Java/Python desktop-pak interoperability and lossless conversion."""
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from data_package import pack_directory, read_data

root=Path(__file__).resolve().parents[3]
source=r'''
package com.zenkaipower.android;
import java.io.*;
import java.nio.file.*;
import java.util.*;
public class GamePakTest {
    static void check(boolean value) { if(!value) throw new AssertionError(); }
    public static void main(String[] args) throws Exception {
        File root=new File(args[0]), fonts=new File(root,"fonts"), pak=new File(root,"fonts.pak");
        check(Arrays.equals(GamePak.read(pak,"original.ttf"),new byte[]{1,2,3}));
        GameFiles.mkdir(fonts);
        Files.write(new File(fonts,"custom.ttf").toPath(),new byte[]{4,5,6});
        Files.write(new File(fonts,"override.ttf").toPath(),new byte[]{9,8,7});
        GamePak.packLoose(fonts);
        check(!fonts.exists());
        check(Arrays.equals(GamePak.read(pak,"original.ttf"),new byte[]{1,2,3}));
        check(Arrays.equals(GamePak.read(pak,"override.ttf"),new byte[]{9,8,7}));
        check(Arrays.equals(GamePak.read(pak,"custom.ttf"),new byte[]{4,5,6}));
        File extract=new File(root,"cache/metadata.bin"); GamePak.extract(pak,"custom.ttf",extract);
        check(Arrays.equals(Files.readAllBytes(extract.toPath()),new byte[]{4,5,6}));
        File broken=new File(root,"broken"); GameFiles.mkdir(broken);
        Files.write(new File(broken,"preserved.txt").toPath(),new byte[]{42});
        Files.write(new File(root,"broken.pak").toPath(),new byte[]{0,1,2});
        try { GamePak.packLoose(broken); throw new AssertionError(); } catch(IOException expected) { }
        check(new File(broken,"preserved.txt").isFile());
        System.out.println("Desktop pak interoperability, overrides, extraction and failed-conversion preservation passed");
    }
}
'''
with tempfile.TemporaryDirectory() as directory:
    folder=Path(directory)
    fonts=folder/'fonts'
    fonts.mkdir()
    (fonts/'original.ttf').write_bytes(bytes([1,2,3]))
    (fonts/'override.ttf').write_bytes(bytes([0,0,0]))
    pack_directory(fonts)
    test=folder/'GamePakTest.java'
    test.write_text(source)
    java=root/'android/app/src/main/java/com/zenkaipower/android'
    subprocess.run(['javac','-d',str(folder),str(java/'GameFiles.java'),str(java/'GamePak.java'),str(java/'GameIO.java'),str(test)],check=True)
    subprocess.run(['java','-cp',str(folder),'com.zenkaipower.android.GamePakTest',str(folder)],check=True)
    assert read_data(fonts/'original.ttf')==bytes([1,2,3])
    assert read_data(fonts/'custom.ttf')==bytes([4,5,6])
    assert read_data(fonts/'override.ttf')==bytes([9,8,7])
