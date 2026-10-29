"""Apply checked VTK 9.2.6 parser workarounds required by MSVC 2015."""
import hashlib
import sys
from pathlib import Path

root = Path(sys.argv[1]) / "Parallel" / "DIY"


def patch(name, old, new):
    path = root / name
    raw = path.read_bytes()
    encoding = "utf-8-sig" if raw.startswith(b"\xef\xbb\xbf") else "utf-8"
    newline = "\r\n" if b"\r\n" in raw else "\n"
    old = old.replace("\n", newline)
    new = new.replace("\n", newline)
    with path.open("r", encoding=encoding, newline="") as stream:
        source = stream.read()
    if new in source:
        if source.count(new) != 1:
            raise RuntimeError("Ambiguous already patched VTK source: " + name)
    elif source.count(old) == 1:
        source = source.replace(old, new)
        with path.open("w", encoding=encoding, newline="") as stream:
            stream.write(source)
    else:
        raise RuntimeError("Unexpected VTK source; refusing to patch " + name)
    assert path.read_bytes() == source.encode(encoding)
    print("VTK v140 parser workaround verified:", name,
          hashlib.sha256(path.read_bytes()).hexdigest())


# QueryPointWorker is concrete: v140 rejects the redundant disambiguator.
patch("vtkDIYGhostUtilities.cxx",
      "queryPointWorker.template operator()<vtkDataArray, double>",
      "queryPointWorker.operator()<vtkDataArray, double>")
# Here Impl is dependent. Keep the standard syntax for all other compilers;
# v140 needs its permissive parser spelling.
old_call = "    return this->Impl.template operator()<DataSetT>(master, assigner, partners);"
patch("vtkDIYGhostUtilities.txx", old_call,
      "#if defined(_MSC_VER) && _MSC_VER == 1900\n"
      "    return this->Impl.operator()<DataSetT>(master, assigner, partners);\n"
      "#else\n" + old_call + "\n#endif")
