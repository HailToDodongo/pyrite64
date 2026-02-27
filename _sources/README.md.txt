# Pyrite64 Docs

This project uses [Sphinx](https://www.sphinx-doc.org) to manage and generate documentation.

## Building Docs

### Setup

Make sure you have python3 installed.<br>
After that, navigate to the directory this readme is in, and create a venv:

```sh
python3 -m venv .venv
source .venv/bin/activate
```

Followed by installing all the dependencies:

```sh
pip install -r requirements.txt
```

### Building

To generate the HTML docs, run:

```sh
make html
```
The final static website is then generated under `_build/html`.<br>
In order to view it properly, a webserver needs to be started.

Navigate into the `_build/html` directory and start a webserver:
```sh
python3 -m http.server
```
This should let you open the page on http://localhost:8000/index.html

After making changes re-run the make command while the server can stay open.

Alternatively, you can run the `./build_and_serve.sh` file to automate this.