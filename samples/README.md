# Debuging nodejs with vscode from sources tree

In order to debug nodejs directly from VScode your should do:

* Set LD_LIBRARY_PATH for nodejs to load libafb-glue.so
* Select nodejs3 (by default vscode generally select nodejs2)

## launch.json

Update vscode debug config with adequate LD_LIBRARY_PATH
```json
        {
            "env": {"LD_LIBRARY_PATH": "${workspaceFolder}/../afb-libglue/build/src:/usr/local/lib64"},
            "name":"nodejs: Current File",
            "type":"nodejs",
            "request":"launch",
            "program":"${file}",
            "console":"integratedTerminal"
        },
```

## select nodejs3

For this click F1 to enter vscode command mode
type: 'nodejs:Select Interpretor' and chose nodejs 3

![vscode nodejs3 debug](./nodejs3-vscode-config.png)

## run debugger

* Open one nodejs from vscode
* Click on debug icon and select 'nodejs: Current File' configuration
* Place breakpoint within your source code
* Start debugging session

`Note: Breakpoint should be introduced before debug session start.`


