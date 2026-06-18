"""
Minimal C++ API page generator (Doxygen XML -> Breathe rST).

Produces, under the given output directory:
  - <root>.rst        : the API landing page, with a toctree of namespaces.
  - <ns-refid>.rst    : one page per namespace. Free functions, variables,
                        typedefs and enums are rendered INLINE here; classes and
                        structs are linked as their own sub-pages via a toctree.
  - <cls-refid>.rst   : one page per class/struct/union (Breathe directive with
                        members), with a toctree to any nested classes.

This intentionally replaces Exhale: Exhale only ever links to a separate page per
function/variable, which is not the layout we want. We reuse the same Doxygen XML
and Breathe directives, just arranged differently.
"""
import os
import shutil
import xml.etree.ElementTree as ET


_CLASS_KINDS = ("class", "struct", "union")
_DIRECTIVE = {
    "class": "doxygenclass",
    "struct": "doxygenstruct",
    "union": "doxygenunion",
    "function": "doxygenfunction",
    "variable": "doxygenvariable",
    "typedef": "doxygentypedef",
    "enum": "doxygenenum",
    "define": "doxygendefine",
}


def _xml_text(el):
    return "".join(el.itertext()).strip() if el is not None else ""


def _heading(title, char):
    return "{0}\n{1}\n".format(title, char * len(title))


class _Gen:
    def __init__(self, xml_dir, out_dir, project):
        self.xml_dir = xml_dir
        self.out_dir = out_dir
        self.project = project
        self.compounds = {}      # refid -> (kind, name)
        self.claimed = set()     # class refids referenced by some parent toctree

    # -- XML helpers ---------------------------------------------------------
    def _parse(self, refid):
        path = os.path.join(self.xml_dir, refid + ".xml")
        if not os.path.exists(path):
            return None
        return ET.parse(path).getroot().find("compounddef")

    def _directive(self, kind, identifier, members=False):
        out = ["", ".. {0}:: {1}".format(_DIRECTIVE[kind], identifier),
               "   :project: {0}".format(self.project)]
        if members:
            out += ["   :members:", "   :protected-members:", "   :undoc-members:"]
        out.append("")
        return "\n".join(out)

    # -- namespace member rendering -----------------------------------------
    def _namespace_members(self, cdef, nsname):
        """Return (functions, typedefs, enums, variables) as rST directive blocks."""
        funcs, typedefs, enums, variables = [], [], [], []
        # collect functions first so overloads can be disambiguated by signature
        raw_funcs = []
        for sec in cdef.findall("sectiondef"):
            for m in sec.findall("memberdef"):
                kind = m.get("kind")
                name = m.findtext("name") or ""
                if not name:
                    continue
                qual = "{0}::{1}".format(nsname, name)
                if kind == "function":
                    types = [_xml_text(p.find("type")) for p in m.findall("param")]
                    raw_funcs.append((name, qual, types))
                elif kind == "typedef":
                    typedefs.append(self._directive("typedef", qual))
                elif kind == "enum":
                    enums.append(self._directive("enum", qual))
                elif kind == "variable":
                    variables.append(self._directive("variable", qual))

        # disambiguate overloaded functions with their parameter-type signature
        counts = {}
        for name, _q, _t in raw_funcs:
            counts[name] = counts.get(name, 0) + 1
        for name, qual, types in raw_funcs:
            if counts[name] > 1:
                ident = "{0}({1})".format(qual, ", ".join(types))
            else:
                ident = qual
            funcs.append(self._directive("function", ident))
        return funcs, typedefs, enums, variables

    # -- page writers --------------------------------------------------------
    def _write(self, refid, text):
        with open(os.path.join(self.out_dir, refid + ".rst"), "w", encoding="utf-8") as f:
            f.write(text)

    def _write_class(self, refid):
        kind, name = self.compounds[refid]
        cdef = self._parse(refid)
        title = "{0} {1}".format(kind.capitalize(), name.split("::")[-1])
        parts = [".. _{0}:\n".format(refid), _heading(title, "="), ""]

        brief = _xml_text(cdef.find("briefdescription")) if cdef is not None else ""
        if brief:
            parts += [brief, ""]

        parts.append(self._directive(kind, name, members=True))

        # nested classes -> their own sub-pages
        nested = []
        if cdef is not None:
            for ic in cdef.findall("innerclass"):
                rid = ic.get("refid")
                if rid in self.compounds:
                    nested.append(rid)
                    self.claimed.add(rid)
        if nested:
            parts += ["", ".. toctree::", "   :maxdepth: 1", ""]
            parts += ["   {0}".format(r) for r in sorted(nested)]
            parts.append("")
        self._write(refid, "\n".join(parts))
        for rid in nested:
            self._write_class(rid)

    def _write_namespace(self, refid):
        kind, name = self.compounds[refid]
        cdef = self._parse(refid)
        if cdef is None:
            return False
        nsname = cdef.findtext("compoundname") or name

        classes = []
        for ic in cdef.findall("innerclass"):
            rid = ic.get("refid")
            if rid in self.compounds:
                classes.append(rid)

        funcs, typedefs, enums, variables = self._namespace_members(cdef, nsname)

        # skip namespaces that carry nothing of interest
        if not (classes or funcs or typedefs or enums or variables):
            return False

        title = "Namespace {0}".format(nsname)
        parts = [".. _{0}:\n".format(refid), _heading(title, "="), ""]
        brief = _xml_text(cdef.find("briefdescription"))
        if brief:
            parts += [brief, ""]

        if classes:
            parts += [_heading("Classes", "-"), "", ".. toctree::", "   :maxdepth: 1", ""]
            parts += ["   {0}".format(r) for r in sorted(classes)]
            parts.append("")
            for rid in classes:
                self.claimed.add(rid)
                self._write_class(rid)

        for section, blocks in (("Functions", funcs), ("Typedefs", typedefs),
                                ("Enums", enums), ("Variables", variables)):
            if blocks:
                parts += [_heading(section, "-"), ""]
                parts += blocks
        self._write(refid, "\n".join(parts))
        return True

    # -- entry point ---------------------------------------------------------
    def run(self, root_name, root_title):
        index = ET.parse(os.path.join(self.xml_dir, "index.xml")).getroot()
        namespaces = []
        for c in index.findall("compound"):
            kind, refid = c.get("kind"), c.get("refid")
            name = c.findtext("name") or ""
            if kind == "namespace" or kind in _CLASS_KINDS:
                self.compounds[refid] = (kind, name)
            if kind == "namespace":
                namespaces.append(refid)

        # fresh output dir (it is .gitignored and fully regenerated)
        if os.path.isdir(self.out_dir):
            shutil.rmtree(self.out_dir)
        os.makedirs(self.out_dir)

        written_ns = [rid for rid in sorted(namespaces, key=lambda r: self.compounds[r][1])
                      if self._write_namespace(rid)]

        # any class not claimed by a namespace/parent gets linked from the root
        orphan_classes = [rid for rid, (k, _n) in self.compounds.items()
                          if k in _CLASS_KINDS and rid not in self.claimed]
        for rid in orphan_classes:
            self._write_class(rid)

        intro = (
            "Reference for the public engine API in ``engine/include/``."
        )
        # maxdepth 1: the overview lists only namespaces, not each namespace's
        # in-page sections (Classes/Functions/...).
        parts = [".. _api_root:\n", _heading(root_title, "="), "", intro, "",
                 ".. toctree::", "   :maxdepth: 1", ""]
        parts += ["   {0}".format(r) for r in written_ns]
        parts += ["   {0}".format(r) for r in sorted(orphan_classes)]
        parts.append("")
        self._write(root_name, "\n".join(parts))


def generate(xml_dir, out_dir, project, root_name="library_root", root_title="C++ API"):
    _Gen(xml_dir, out_dir, project).run(root_name, root_title)
