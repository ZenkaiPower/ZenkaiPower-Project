"""Check production touch-button output merging using Java (JDK 11+)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
view = (root/'android/app/src/main/java/com/zenkaipower/android/VirtualPadView.java').read_text()
methods = view[view.index('    private void held('):view.index('    private void syncButtons(')]
assert '"triangle", KeyEvent.KEYCODE_BUTTON_Y, -1,\n                "r1", KeyEvent.KEYCODE_BUTTON_R1' in view
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
        int key,axis,directionKey; boolean held,wanted,sequential,secondHeld;
        long pressedAt,secondPressedAt,animationUntil;
        Runnable release,secondPress;
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
    static class SystemClock {
        static long now;
        static long uptimeMillis() { return now; }
    }
    final Map<Runnable,Long> callbacks=new LinkedHashMap<>();
    void postDelayed(Runnable task,long delay) { callbacks.put(task,SystemClock.now+delay); }
    void removeCallbacks(Runnable task) { callbacks.remove(task); }
    void invalidate() {}
    void advance(long time) {
        while(true) {
            Runnable next=null; long due=Long.MAX_VALUE;
            for(var entry:callbacks.entrySet()) if(entry.getValue()<due) {
                next=entry.getKey(); due=entry.getValue();
            }
            if(next==null || due>time) break;
            callbacks.remove(next); SystemClock.now=due; next.run();
        }
        SystemClock.now=time;
    }
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
        Button combo=new Button(100,-1,103), triangle=new Button(100,-1,0);
        combo.sequential=true;
        Collections.addAll(pad.buttons,combo,triangle);
        // Even a very short tap has Triangle-only, then both, then neither.
        pad.request(combo,true);
        check(TouchPadBridge.keys.get(100) && !TouchPadBridge.keys.getOrDefault(103,false));
        pad.advance(5); pad.request(combo,false);
        pad.advance(39); pad.updatePadButtons();
        check(TouchPadBridge.keys.get(100) && !TouchPadBridge.keys.getOrDefault(103,false));
        pad.advance(40);
        check(TouchPadBridge.keys.get(100) && TouchPadBridge.keys.get(103));
        pad.advance(79); check(TouchPadBridge.keys.get(103));
        pad.advance(80);
        check(!TouchPadBridge.keys.get(100) && !TouchPadBridge.keys.getOrDefault(103,false));
        // A hold keeps both down; shared Triangle survives the combo's release.
        pad.request(combo,true); pad.advance(120); pad.advance(500);
        check(TouchPadBridge.keys.get(100) && TouchPadBridge.keys.get(103));
        pad.request(triangle,true); pad.request(combo,false); pad.advance(500);
        check(TouchPadBridge.keys.get(100) && !TouchPadBridge.keys.getOrDefault(103,false));
        pad.request(triangle,false); pad.advance(540);
        // Cancellation before R1 is due must discard the pending press.
        pad.request(combo,true); pad.held(combo,false); pad.advance(600);
        check(!TouchPadBridge.keys.get(100) && !TouchPadBridge.keys.getOrDefault(103,false));
        check(pad.callbacks.isEmpty());
        // A late UI callback must still leave both buttons down for 40 ms.
        pad.request(combo,false); pad.request(combo,true); pad.request(combo,false);
        Runnable second=combo.secondPress;
        pad.callbacks.remove(second); SystemClock.now=670; second.run();
        pad.advance(709); check(TouchPadBridge.keys.get(103));
        pad.advance(710);
        check(!TouchPadBridge.keys.get(100) && !TouchPadBridge.keys.get(103));
        System.out.println("Touch combos: shared buttons, sequential taps/holds, cancellation and late callbacks passed");
    }
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)/'TouchComboTest.java'
    path.write_text(source)
    subprocess.run(['java', str(path)], check=True)
