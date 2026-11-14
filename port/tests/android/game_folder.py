"""Exercise desktop-style game-folder setup and non-overwriting migration (JDK)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = r'''
package com.zenkaipower.android;
import java.io.*;
import java.nio.file.*;
public class GameFolderTest {
    static void check(boolean value) { if(!value) throw new AssertionError(); }
    static void write(File root,String name,String value) throws Exception {
        File file=new File(root,name); GameFiles.mkdir(file.getParentFile());
        Files.writeString(file.toPath(),value);
    }
    static void rejected(File volume,String path) throws Exception {
        try { GameFiles.resolveFolder(volume,path); throw new AssertionError(path); }
        catch(IOException expected) { }
    }
    public static void main(String[] args) throws Exception {
        File volume=new File(args[0]);
        File from=new File(volume,"old"), to=GameFiles.resolveFolder(volume,"ZenkaiPower");
        GameFiles.prepare(from); GameFiles.prepare(to);
        check(java.util.Arrays.equals(GameFiles.DIRECTORIES,new String[]{"gamedata","licenses","logs","net_session","savegame","setup","textures","tools"}));
        for(String name:GameFiles.DIRECTORIES) check(new File(to,name).isDirectory());
        check(!GameFiles.ready(to));
        write(from,"gamedata/disc.pak","disc"); write(from,"gamedata/game-text.pak","text");
        write(from,"savegame/player.sav","old save"); write(from,"savegame/other.sav","other save");
        write(from,"gamedata/touch-buttons/up.png","arrow"); write(from,"textures/custom.png","texture");
        write(from,"gamedata/mods/custom.bin","mod"); write(from,"private-unrelated.txt","excluded");
        write(to,"savegame/player.sav","new save");
        check(GameFiles.ready(from));
        GameFiles.copyInstallation(from,to,name -> {});
        check(GameFiles.ready(to));
        check(Files.readString(new File(to,"savegame/player.sav").toPath()).equals("new save"));
        check(Files.readString(new File(to,"savegame/other.sav").toPath()).equals("other save"));
        check(new File(to,"gamedata/touch-buttons/up.png").isFile());
        check(new File(to,"textures/custom.png").isFile() && new File(to,"gamedata/mods/custom.bin").isFile());
        check(!new File(to,"private-unrelated.txt").exists());
        GameFiles.copyInstallation(from,to,name -> {throw new AssertionError("Re-copied "+name);});
        File loose=new File(volume,"loose"); GameFiles.prepare(loose);
        write(loose,"gamedata/disc/SLUS_216.78","exe"); write(loose,"gamedata/disc/BIN/DBZP.BIN","menu");
        write(loose,"gamedata/game-text/labels.json","{}"); check(GameFiles.ready(loose));
        rejected(volume,""); rejected(volume,"../outside"); rejected(volume,"Android/data/game");
        rejected(volume,"ZenkaiPower/gamedata"); rejected(volume,"ZenkaiPower/savegame");
        try { GameFiles.copyInstallation(from,new File(from,"nested"),name -> {}); throw new AssertionError(); }
        catch(IOException expected) { }
        check(new File(from,"savegame/player.sav").isFile());
        System.out.println("Game folders: setup, packed/loose data, migration, save preservation and path validation passed");
    }
}
'''
with tempfile.TemporaryDirectory() as directory:
    folder = Path(directory)
    test = folder/'GameFolderTest.java'
    test.write_text(source)
    production = root/'android/app/src/main/java/com/zenkaipower/android/GameFiles.java'
    subprocess.run(['javac','-d',str(folder),str(production),str(production.with_name('GamePak.java')),str(production.with_name('GameIO.java')),str(test)],check=True)
    subprocess.run(['java','-cp',str(folder),'com.zenkaipower.android.GameFolderTest',str(folder/'storage')],check=True)
