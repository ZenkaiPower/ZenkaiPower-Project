"""Check production touch-button output merging using Java (JDK 11+)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
view = (root/'android/app/src/main/java/com/zenkaipower/android/VirtualPadView.java').read_text()
methods = view[view.index('    private void held('):view.index('    private void request(')]
source = '''
import java.util.*;
class TouchComboTest {
    static class SparseIntArray {
        final Map<Integer,Integer> values=new HashMap<>();
        int get(int key,int fallback) { return values.getOrDefault(key,fallback); }
        void put(int key,int value) { values.put(key,value); }
        void clear() { values.clear(); }
    }
    static class Button {
        int key,axis,directionKey; boolean held;
        Button(int key,int axis,int directionKey) {
            this.key=key; this.axis=axis; this.directionKey=directionKey;
        }
    }
    static class TouchPadBridge {
        static Map<Integer,Boolean> keys=new HashMap<>();
        static Map<Integer,Float> axes=new HashMap<>();
        static void button(int key,boolean down) { keys.put(key,down); }
        static void axis(int axis,float value) { axes.put(axis,value); }
    }
    final ArrayList<Button> buttons=new ArrayList<>();
    final SparseIntArray outputKeys=new SparseIntArray();
    final SparseIntArray wantedKeys=new SparseIntArray();
''' + methods + '''
    static void check(boolean value) { if(!value) throw new AssertionError(); }
    public static void main(String[] args) {
        TouchComboTest pad=new TouchComboTest();
        Button l2=new Button(0,4,0), up=new Button(0,4,19), down=new Button(0,4,20);
        Button cross=new Button(96,-1,0);
        Collections.addAll(pad.buttons,l2,up,down,cross);
        pad.held(up,true);
        check(TouchPadBridge.axes.get(4)==1 && TouchPadBridge.keys.get(19));
        pad.updatePadButtons(); // Battle polling must preserve a combo's trigger.
        check(TouchPadBridge.axes.get(4)==1);
        pad.held(l2,true); pad.held(up,false);
        check(TouchPadBridge.axes.get(4)==1 && !TouchPadBridge.keys.get(19));
        pad.held(down,true); pad.held(l2,false);
        check(TouchPadBridge.axes.get(4)==1 && TouchPadBridge.keys.get(20));
        pad.held(up,true); pad.held(down,false);
        check(TouchPadBridge.axes.get(4)==1 && !TouchPadBridge.keys.get(20));
        pad.held(cross,true); pad.held(up,false);
        check(TouchPadBridge.axes.get(4)==-1 && !TouchPadBridge.keys.get(19));
        check(TouchPadBridge.keys.get(96));
        pad.held(cross,false);
        check(!TouchPadBridge.keys.get(96) && TouchPadBridge.axes.get(5)==-1);
        check(!TouchPadBridge.keys.containsKey(0));
        System.out.println("Touch combos: D-pad, shared L2, polling and independent buttons passed");
    }
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)/'TouchComboTest.java'
    path.write_text(source)
    subprocess.run(['java', str(path)], check=True)
