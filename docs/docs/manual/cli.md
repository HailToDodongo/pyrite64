# CLI

The editor can also be used in a command-line interface (CLI) mode.<br>
This can be useful to build projects automatically and without requiring user interactions / a GPU to be present.<br>
Common use-cases are:
- Build Task in an IDE (e.g. when only editing C++ code)
- CI/CD pipelines
- Remote building on a server

To see all available CLI options, run:
```bash
./pyrite64 --help
```

As an example: to build a given project you can run:
```bash
./pyrite64 --cli --cmd build /path/to/project.p64proj
```

```{admonition} Tip
:class: info

It's perfectly safe to run the CLI while the visual editor is open 
```

If no arguments are given, or only the path to a project file, the visual editor will be launched instead.