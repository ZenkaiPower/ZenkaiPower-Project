"""Reject malformed DOL metadata before nod slices or allocates its buffer."""
from pathlib import Path
import sys

path = Path(sys.argv[1])/'nod/src/disc/gcn.rs'
source = path.read_text()
if '// ZenkaiPower: bounded DOL metadata' not in source:
    old = '.map(|(offs, size)| offs.get() + size.get())'
    if source.count(old) != 1: raise SystemExit('nod DOL reader changed; review the bounds patch')
    source = source.replace(old, '.map(|(offs, size)| offs.get().saturating_add(size.get()))')
    old = '    let mut raw_dol = <[u8]>::new_box_zeroed_with_elems(dol_size as usize)?;'
    if source.count(old) != 1: raise SystemExit('nod DOL allocation changed; review the bounds patch')
    source = source.replace(old, '''    // ZenkaiPower: bounded DOL metadata; malformed discs return errors across FFI.
    if dol_size < size_of::<DolHeader>() as u32 || dol_size > 64 * 1024 * 1024 {
        return Err(crate::Error::DiscFormat("Invalid DOL size".to_string()));
    }
''' + old)
    path.write_text(source)
