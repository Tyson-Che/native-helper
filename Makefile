CC=clang
CFLAGS=-std=c11 -Wall -Wextra -pedantic
FRAMEWORKS=-framework ApplicationServices -framework CoreFoundation -framework CoreServices
TARGET=native-helper
SOURCES=main.c cjson.c

$(TARGET): $(SOURCES)
$(CC) $(CFLAGS) $(SOURCES) $(FRAMEWORKS) -o $@

clean:
rm -f $(TARGET)
