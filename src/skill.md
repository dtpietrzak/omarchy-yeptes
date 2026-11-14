---
name: yeptes
description: Work with Yeptes, the plain text notepad on this desktop, and its .ypts notes. Use when asked to write, check, or open a Yeptes note, or to open a text, Markdown, JSON, or HTML file in Yeptes.
---

# Yeptes

Yeptes is a plain text notepad. It opens any UTF-8 text file and shows it as
monospace text, rendered Markdown, a folding JSON tree, static HTML, a table
(CSV, TSV), syntax-colored code, or a diff of its changes.

## Notes (`.ypts`)

A `.ypts` note is plain UTF-8 text. The one thing special about it is that
fenced blocks are syntax colored in the text view:

````text
Some notes.

``` json
{ "colored": true }
```
````

The fence opens with three backticks and a language name on a line of its own
and closes with three backticks alone. Languages: bash, c, cpp, csharp, css,
go, html, java, javascript, json, kotlin, lua, markdown, php, python, ruby,
rust, sql, swift, toml, typescript, xml, yaml, and the usual short names
(`js`, `ts`, `py`, `c++`, `c#`, `sh`, `md`, `yml`, `rs`).

A note that has been changed and saved in Yeptes ends with a history: a
marker line that starts with the control character U+001E and reads
`yeptes history: ...`, then one JSON object per saved change. Edit the text
above that line with your normal file tools and leave the marker and
everything after it exactly as it is. A note you write from scratch needs no
history. A line such as `- [ ] task` is a checklist item the user can tick.

## Commands

- `yeptes check <file> [--json]` checks that a file will open (UTF-8, under
  16 MB), that a `.json` file parses, and lists the fences of a `.ypts` note
  with whether each language is colored, and counts its saved changes. Exits 1 with the problem on stderr.
- `yeptes open <file>` opens the window on a file. It needs the desktop, so
  only run it when the user asks to see the file.
