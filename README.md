# fox-lang

\- Reinvent the PHP -

## Important notice

For windows users, please set "Configure the line ending conversions" as false (Checkout as-is commit as-is).

```
git config --global core.autocrlf false
```

fox language do not accept CRLF line ending.

## What's this

fox language is a dynamic and strong typed programming language.
It works on Windows, MacOSX and Linux.

### Language spec

- Commonplace grammer (if, for, while)
- Multiple precision arithmetic (Integer and Rational number)
- Strings are always UTF-8 encoded
- Automatic resource mangement (Reference counting)

### Libraries available

- iconv text convertion
- Read/Write images (JPEG, GIF, PNG, Windows BMP, WebP)
- Read/Write audio (WAV)
- sqlite3
- ZIP archive operation
- Compress/Decompress XZ
- Windows OLE

## Getting started

Following versions are available.

### Windows (64bit)

https://x768.sakura.ne.jp/fox-lang/fox-win64.zip

Just unzip it and add %PATH% to fox.exe directory.

### MacOSX

https://x768.sakura.ne.jp/fox-lang/fox-osx.dmg

### Linux (GTK+3)

Following libraries are required.

- PCRE
- iconv
- GTK+3
- sqlite3
- zlib
- openssl
- libpng
- libjpeg

## How to use it

```
$ cat test.fox
for i in 1...3 {
  puts i
}
$ fox test.fox
1
2
3
$ fox -e "for i in 1...3 { puts i }"
1
2
3
```
