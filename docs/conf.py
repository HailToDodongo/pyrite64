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

extensions = ['myst_parser']

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

# https://alabaster.readthedocs.io/en/latest/customization.html
html_theme = 'furo'
html_static_path = ['_static']


html_css_files = [
    'custom.css',
]

html_theme_options = {
  "sidebar_hide_name": True,    
  "light_logo": 'logo.png',
  "dark_logo": 'logo.png',
  
  "light_css_variables": {
    "font-stack": "Noto, Arial, sans-serif",
  },
} 
