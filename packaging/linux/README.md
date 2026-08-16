# SumatraPDF for Linux

This archive contains the native GTK4 SumatraPDF application. It is portable in the sense that it can be extracted
without an installer; it uses the GTK4 and system libraries supplied by the Linux distribution.

Run it directly from the extracted directory:

```sh
./bin/SumatraPDF [document.pdf]
```

To install it for the current user, copy the executable and desktop resources into the corresponding XDG locations:

```sh
install -Dm755 bin/SumatraPDF "$HOME/.local/bin/SumatraPDF"
mkdir -p "$HOME/.local/share"
cp -r share/* "$HOME/.local/share/"
```

The initial Linux reader registers PDF support. Its runtime dependencies include GTK 4, GLib/GIO, Cairo, Pango,
OpenSSL, and their normal platform dependencies.
