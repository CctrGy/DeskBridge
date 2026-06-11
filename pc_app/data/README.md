# DeskBridge PC App Data

This directory stores non-code files used by the PC application.

Use it for runtime data and companion files such as:

- configuration files
- saved logs
- state snapshots
- presets
- generated exports

Keep Python source code outside this directory.

`data.config` stores runtime settings in DeskBridge text format. Currently:

```text
{
  last_connection: None
  log_level: INFO
  language: english
  terminal_palette: default
  history_limit: 200
  history_visual_size: 25
}
```

`last_connection` is updated after a valid DeskBridge USB connection. On the
next start the PC app tries that COM port first, but only uses it if VID/PID
still match `1209:DB01`.

`log_level` controls the daily log file in `data/logs/DD-MM-YYYY.log`.
Valid values are `DEBUG`, `INFO`, `USER`, `WARNING`, `ERROR`, and `CRITICAL`.

`language` stores the selected shell language. Language files are named
`shell_<language>.json` inside `data/settings/languaje`.

`terminal_palette` selects a palette from `data/settings/terminal_palete.json`.

`history_limit` is the maximum remembered command count. `history_visual_size`
is the number of commands shown by the `history` command.
