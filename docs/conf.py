# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'Pyrite64'
copyright = '2026, Max Bebök'
author = 'Max Bebök'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

import os
import subprocess

# Fast dev mode (set PYRITE_DOCS_FAST=1): skips the C++ API generation entirely.
# Building the ~250 Exhale/Breathe API pages dominates build time (~2 minutes),
# while the rest of the manual builds in seconds. Use this while iterating on the
# manual or CSS; do a full build (default) when you need the API or before deploy.
_fast = os.environ.get("PYRITE_DOCS_FAST") == "1"

# Within a FULL build, the watch loop sets this when a change doesn't touch any
# C++ API input (e.g. CSS or a manual page). Breathe and the API pages stay
# enabled, but the slow Doxygen + _apigen regeneration is skipped and the already
# generated pages/XML are reused. See build_and_serve.sh.
_skip_doxygen = os.environ.get("PYRITE_DOCS_SKIP_DOXYGEN") == "1"

extensions = ['myst_parser']
if not _fast:
    extensions += ['breathe']

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', '.venv']

# Exhale gives nested classes both their own page and an inline declaration on the
# parent's page; the strict C++ domain flags that as a duplicate. It's harmless
# here, so silence just that subtype.
suppress_warnings = ['duplicate_declaration.cpp']

if _fast:
    # The API pages don't exist in fast mode; drop them and silence the toctree
    # reference to them from the user manual.
    exclude_patterns += ['docs/manual/api']
    suppress_warnings += ['toc', 'ref']
    print("[conf.py] FAST mode: skipping C++ API (Doxygen/Breathe/Exhale).")

# -- C++ API docs (Doxygen XML -> Breathe -> Exhale) -------------------------
# Skipped entirely in fast mode (see above).
if not _fast:
    # Doxygen is run automatically here as a silent XML backend; it never produces
    # its own site. Breathe then renders that XML inside this Sphinx build.
    _docs_dir = os.path.abspath(os.path.dirname(__file__))
    _doxygen_xml = os.path.join(_docs_dir, "_build", "doxygen", "xml")

    os.makedirs(_doxygen_xml, exist_ok=True)

    # Run on every build so the API docs always match the headers. Skip if the
    # doxygen binary is missing (e.g. minimal env) but XML already exists.
    # In SKIP_DOXYGEN mode we reuse the existing XML/pages from the last full build.
    if _skip_doxygen:
        print("[conf.py] SKIP_DOXYGEN: reusing existing C++ API "
              "(no Doxygen/_apigen run).")
    else:
        try:
            subprocess.run(["doxygen", "Doxyfile"], cwd=_docs_dir, check=True)
        except (FileNotFoundError, subprocess.CalledProcessError) as err:
            if not os.path.isdir(_doxygen_xml):
                raise
            print(f"[conf.py] WARNING: doxygen not run ({err}); using existing XML.")

    breathe_projects = {"pyrite64": _doxygen_xml}
    breathe_default_project = "pyrite64"
    breathe_default_members = ("members",)

    # Generate the API page tree from the Doxygen XML: a root page, a page per
    # namespace (free functions/variables/typedefs/enums rendered inline), and a
    # separate page per class/struct linked as sub-pages. See _apigen.py.
    # Skipped in SKIP_DOXYGEN mode; the pages from the last full build are reused.
    if not _skip_doxygen:
        import sys
        sys.path.insert(0, _docs_dir)
        import _apigen
        _apigen.generate(
            xml_dir=_doxygen_xml,
            out_dir=os.path.join(_docs_dir, "docs", "manual", "api"),
            project="pyrite64",
        )

    # Wrap long C++ signatures onto multiple lines (one parameter per line) instead
    # of one cramped horizontal line. Pairs with the API card styling in custom.css.
    cpp_maximum_signature_line_length = 90
    maximum_signature_line_length = 90


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

# https://alabaster.readthedocs.io/en/latest/customization.html
html_theme = 'furo'
html_static_path = ['_static']
html_favicon = '_static/favicon.ico'

html_css_files = [
    'custom.css',
]

html_theme_options = {
  "sidebar_hide_name": True,
  "light_logo": 'logo.png',
  "dark_logo": 'logo.png',

  "light_css_variables": {
    "font-stack": "Noto, Arial, sans-serif",
    # C++ API token colors (light mode)
    "color-api-name": "#F5A937",       # symbol name (e.g. addObject)
    "color-api-pre-name": "#5a6b7b",   # namespace / class qualifier
    "color-api-keyword": "#a626a4",    # const, class, inline, template...
    "color-api-paren": "#5a6b7b",      # parentheses
    "color-api-background": "#f0f2f4", # signature header background
  },
  "dark_css_variables": {

    "color-api-name": "#F5A937",
    "color-api-pre-name": "#9aa7b4",
    "color-api-keyword": "#d6a0ff",
    "color-api-paren": "#9aa7b4",
    "color-api-background": "#222225",
  },
}
