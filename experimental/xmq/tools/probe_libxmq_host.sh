#!/usr/bin/env bash
# Probe upstream libxmq portability without touching the production firmware.
set -euo pipefail
SHA="293fd38cdcca176091c789c33a8797d67471c060"
ROOT="https://raw.githubusercontent.com/wmbusmeters/wmbusmeters/${SHA}/src"
mkdir -p /tmp/wmbus-xmq-probe
cd /tmp/wmbus-xmq-probe
curl --fail --location --retry 3 --silent --show-error "${ROOT}/xmq.c" -o xmq.c
curl --fail --location --retry 3 --silent --show-error "${ROOT}/xmq.h" -o xmq.h
# Compile upstream amalgamation against host libxml2 headers. This does not prove ESP-IDF portability.
printf 'Pinned upstream commit: %s\n' "$SHA"
wc -c xmq.c xmq.h
printf '\nHeaders included by xmq.c:\n'
grep -E '^#[[:space:]]*include' xmq.c | sort -u || true
printf '\nBuilding with non-XML/JSON/printing options...\n'
gcc -std=gnu11 -O2 -Wall -Wno-unused-function \
  -DXMQ_NO_LIBXML -DXMQ_NO_JSON -DXMQ_NO_XMQ_PRINTING \
  -I. $(pkg-config --cflags libxml-2.0) -c xmq.c -o xmq.o
printf '\nCompiled object sizes:\n'
size xmq.o
printf '\nUndefined external symbols (portability inventory):\n'
nm -u xmq.o || true

printf '\nLinking and executing XMQ parser smoke test...\n'
gcc -std=gnu11 -O2 -I. $(pkg-config --cflags libxml-2.0) \
  -c -x c -o xmq_parse_smoke.o - <<'EOF'
#include "xmq.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(void) {
  const char *sample = "driver { name = smoke meter_type = WaterMeter }";
  XMQReturnDoc rd = xmqNewDoc();
  if (rd.status != XMQ_OK || !rd.doc) return 10;
  if (!xmqParseBuffer(rd.doc, sample, sample + strlen(sample), NULL, 0)) return 11;
  const char *name = xmqGetString(rd.doc, "/driver/name");
  if (!name || strcmp(name, "smoke") != 0) {
    fprintf(stderr, "XMQ parsed but name lookup failed: %s\\n", name ? name : "(null)");
    return 12;
  }
  puts("libxmq host parse and lookup: PASS");
  return 0;
}
EOF
gcc xmq_parse_smoke.o xmq.o $(pkg-config --libs libxml-2.0) -lm -o xmq_parse_smoke
./xmq_parse_smoke

printf '\nParsing original upstream Techem XMQ driver definitions...\n'
DRIVER_DIR="${GITHUB_WORKSPACE:-$(pwd)}/experimental/xmq/drivers"
# Keep the full GitHub workspace path available after changing to /tmp above.
gcc -std=gnu11 -O2 -I. $(pkg-config --cflags libxml-2.0) \
  -c -x c -o xmq_techem_test.o - <<'EOF'
#include "xmq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int check(const char *filename, const char *expected) {
  FILE *f = fopen(filename, "rb");
  if (!f) { perror(filename); return 20; }
  if (fseek(f, 0, SEEK_END)) return 21;
  long length = ftell(f);
  if (length <= 0 || fseek(f, 0, SEEK_SET)) return 22;
  char *buf = (char *)malloc((size_t)length + 1);
  if (!buf) return 23;
  if (fread(buf, 1, (size_t)length, f) != (size_t)length) return 24;
  fclose(f);
  buf[length] = 0;
  XMQReturnDoc rd = xmqNewDoc();
  if (rd.status != XMQ_OK || !rd.doc) return 25;
  if (!xmqParseBuffer(rd.doc, buf, buf + length, NULL, 0)) {
    fprintf(stderr, "Could not parse %s: %s\n", filename, xmqDocError(rd.doc));
    return 26;
  }
  const char *name = xmqGetString(rd.doc, "/driver/name");
  if (!name || strcmp(name, expected)) {
    fprintf(stderr, "%s: expected %s, got %s\n", filename, expected, name ? name : "(null)");
    return 27;
  }
  int tests = xmqForeach(rd.doc, "/driver/tests/test", NULL, NULL);
  if (tests <= 0) { fprintf(stderr, "%s: no tests found\n", filename); return 28; }
  printf("%s: parsed name=%s, test fixtures=%d\n", filename, name, tests);
  free(buf);
  return 0;
}
int main(int argc, char **argv) {
  if (argc != 3) return 2;
  if (check(argv[1], "fhkvdataiii")) return 3;
  if (check(argv[2], "mkradio4")) return 4;
  puts("Techem XMQ driver definitions: PASS");
  return 0;
}
EOF
gcc xmq_techem_test.o xmq.o $(pkg-config --libs libxml-2.0) -lm -o xmq_techem_test
./xmq_techem_test "${DRIVER_DIR}/fhkvdataiii.xmq" "${DRIVER_DIR}/mkradio4.xmq"
