# Ptyxis non-Latin Shortcut Fix

Форк [Ptyxis](https://gitlab.gnome.org/GNOME/ptyxis) (на базе версии 50.1) с исправлением работы клавиатурных шорткатов при не-латинских раскладках (кириллица и др.).

## Проблема

Шорткаты копирования/вставки (`Ctrl+Shift+C` и `Ctrl+Shift+V`) в терминале не срабатывали, если активна не-латинская раскладка клавиатуры: сопоставление выполнялось по символу (keyval), который сгенерировало нажатие, а не по физической клавише.

## Исправление

Сопоставление горячих клавиш выполняется по физической клавише (keycode), а не по символу. Для этого шорткат раскладывается обратно в набор физических клавиш (`gdk_display_map_keyval`), и событие нажатия сравнивается с ними. Теперь `Ctrl+Shift+C`/`Ctrl+Shift+V` работают независимо от текущей раскладки.

### Затронутые файлы

- `src/ptyxis-terminal.c`
  - `ptyxis_terminal_keycode_matches_accel()` — новая функция: сопоставление события нажатия со шорткатом по keycode.
  - `ptyxis_terminal_capture_key_pressed_cb()` — вызов нового механизма для действий `copy-clipboard` и `paste-clipboard`.

## Сборка

```bash
meson setup _build --prefix=/usr/local --buildtype=release
meson compile -C _build
sudo meson install -C _build
```

## Лицензия

GPL v3+ (см. файл `COPYING`).